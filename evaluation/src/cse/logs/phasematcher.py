#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
These classes reads JSON data from a variety sources and parse them into
objects that support a `from_dict` method.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import gzip
import json

from collections import defaultdict
from datetime import datetime, timezone

from cse.types.analyzer import Analyzer

# -- packet keys --------------------------------------------------------------

packet_keys = ["isyn", "iack", "rsyn", "rack"]


# -- time ---------------------------------------------------------------------


def get_unix_timestamp(datetime_object):
    ts = datetime_object.astimezone(timezone.utc)
    return int(datetime.timestamp(ts))


def is_within_timeout(con, ts_syn, to):
    if con is None:
        return False
    ts_con = get_unix_timestamp(con.timestamp)
    # SYN must be received within to secs of the probe confirmation.
    return (ts_con <= ts_syn and (ts_syn - ts_con) < to) or (
        ts_syn < ts_con and (ts_con - ts_syn) <= 1
    )


# -- syn matching -------------------------------------------------------------


def is_matching_ack(syn, req, ack):
    # Check source port which is not part of the key, but should match
    # between SYN and its ACK.
    if (
        req is not None
        and syn.payload.sport == req.dport == ack.payload.sport
        and req.anum == ack.payload.snum
    ):
        return True
    return False


# -- count list over keys -----------------------------------------------------


def count(collection):
    cnt = 0
    for val in collection.values():
        cnt += len(val)
    return cnt


def count_unanswered(collection):
    cnt = 0
    for val in collection.values():
        cnt += len([v for v in val if v.probe_request is None])
    return cnt


# -- filtering tuples -----------------------------------------------------


def filter_list(lst, batch_id, tag):
    removed = []
    keys_to_delete = []
    for key, evs in lst.items():
        keep = []
        for ev in evs:
            if ev.batch_id == batch_id:
                d = {
                    "ts": get_unix_timestamp(ev.packet.timestamp),
                    "isyn": None,
                    "iack": None,
                    "rsyn": None,
                    "rack": None,
                    "tag": tag,
                }
                d[tag] = ev.to_dict()
                removed.append(d)
            else:
                keep.append(ev)
        if len(keep) == 0:
            keys_to_delete.append(key)
        elif len(evs) != len(keep):
            lst[key] = keep
    for key in keys_to_delete:
        del lst[key]
    return removed


def filter_dict_elems(lst, batch_id, tag, key_with_packet):
    removed = []
    keys_to_delete = []
    for key, evs in lst.items():
        keep = []
        for ev in evs:
            pkt = ev[key_with_packet]
            if pkt.batch_id == batch_id:
                d = {
                    "ts": get_unix_timestamp(pkt.packet.timestamp),
                    "tag": tag,
                }
                for default_key in packet_keys:
                    if default_key not in ev or ev[default_key] is None:
                        d[default_key] = None
                    else:
                        d[default_key] = ev[default_key].to_dict()
                removed.append(d)
            else:
                keep.append(ev)
        if len(keep) == 0:
            keys_to_delete.append(key)
        elif len(evs) != len(keep):
            lst[key] = keep
    for key in keys_to_delete:
        del lst[key]
    return removed


# -- phase matching -----------------------------------------------------------


class PhaseMatcher:
    def __init__(self, phase_timeout):
        # Single Event objects.
        self.irregular_syns = defaultdict(lambda: [])
        self.regular_syns = defaultdict(lambda: [])
        self.acks = defaultdict(lambda: [])

        # Dict with keys 'isyn', 'iack'.
        self.irregular_acked = defaultdict(lambda: [])
        # Dict with keys 'rsyn', 'rack'.
        self.regular_acked = defaultdict(lambda: [])

        # Dict with keys 'isyn', 'iack', and 'rsyn', 'rack'.
        # The ack keys might be None.
        self.two_phase_no_ack = defaultdict(lambda: [])
        # The key 'iack' might be None.
        self.two_phase = defaultdict(lambda: [])

        # Dict as above, but includes an 'ident' member.
        self.repeated_connection_attempts = defaultdict(lambda: [])

        # First idea:
        #  batch_id -> (saddr, daddr, sport, dport) -> snum
        # But that doesn't work because we can see retransmits over the "edges"
        # of batches we read in.
        self.retransmit_cache = defaultdict(lambda: set())
        # retransmit_cache = defaultdict(lambda: defaultdict(lambda: 0))
        self.retransmits = defaultdict(lambda: [])

        self.phase_timeout_s = int(phase_timeout.total_seconds())

    # -- event processing -----------------------------------------------------

    def add_event(self, event):
        if "rst" in event.packet.payload.flags:
            return
        # Filter retransmits.
        if self.is_retransmit(event):
            self.retransmits[event.batch_id].append(event)
            return
        # Do things ...
        lyz = Analyzer(event.packet)
        key = event.get_key()
        if lyz.is_irregular_syn():
            # Irregular SYN
            self.irregular_syns[key].append(event)
        elif lyz.is_syn():
            # Regular SYN.
            # Match against irregular SYN within the interval.
            if not self.try_match_rsyn_to_isyn(event, key):
                # Match against irregular SYN with ACK within the interval.
                if not self.try_match_rsyn_to_isyn_acked(event, key):
                    self.regular_syns[key].append(event)
        elif lyz.is_ack():
            # ACKs.
            # Check regular SYNs (more likely)
            if not self.try_match_ack_to_rsyn(event, key):
                # Check irregular SYNs (less likely)
                if not self.try_match_ack_to_isyn(event, key):
                    # Check two-phase event without ACK.
                    if not self.try_match_ack_to_tp(event, key):
                        # Just save it.
                        self.acks[key].append(event)
        elif lyz.is_syn_ack():
            # Nothing to do?
            pass
        elif lyz.is_rst():
            # Nothing to do?
            pass
        else:
            # flags = ", ".join(event.packet.payload.flags)
            # print(
            # f"unknown packet with flags '{flags}': "
            # f"{json.dumps(event.packet.to_dict())}"
            # )
            # TODO: Try to match these?
            pass

    def try_match_everything(self):
        # - ACKS
        matched_later = 0
        for key, ack_list in self.acks.items():
            remaining = []
            for ack in ack_list:
                # Most likely?
                if not self.try_match_ack_to_rsyn(ack, key):
                    # Check irregular SYNs (less likely)
                    if not self.try_match_ack_to_isyn(ack, key):
                        # Check two-phase event without ACK.
                        if not self.try_match_ack_to_tp(ack, key):
                            # Just save it.
                            remaining.append(ack)
                        else:
                            matched_later += 1
                    else:
                        matched_later += 1
                else:
                    matched_later += 1
            after = len(remaining)
            self.acks[key] = remaining
            assert len(self.acks[key]) == after, "jo, what?"
        print(f"[PHASE] matched {matched_later} ACKS")

        # - Phases
        matched_later = 0
        for key, rsyns in self.regular_syns.items():
            remaining = []
            matched_index = None
            for i, rsyn in enumerate(rsyns):
                if self.try_match_rsyn_to_isyn(rsyn, key):
                    matched_index = i
                    matched_later += 1
                    break
                if self.try_match_rsyn_to_isyn_acked(rsyn, key):
                    matched_index = i
                    matched_later += 1
                    break
            if matched_index is not None:
                del rsyns[i]
        print(f"[PHASE] matched {matched_later} phases (rsyns)")

        matched_later = 0
        for key, rseqs in self.regular_acked.items():
            remaining = []
            matched_index = None
            # This could be the second phase.
            for i, rseq in enumerate(rseqs):
                if self.try_match_rsyn_acked_to_isyn(rseq, key):
                    matched_index = i
                    matched_later += 1
                    break
                if self.try_match_rsyn_acked_to_isyn_acked(rseq, key):
                    matched_index = i
                    matched_later += 1
                    break
            if matched_index is not None:
                del rseqs[i]
        print(f"[PHASE] matched {matched_later} phases (acked rsyns)")

    def find_repeated_connections(self):
        before = count(self.regular_acked)
        now_empty_keys = []
        total_deleting = 0
        for key, rseqs in self.regular_acked.items():
            keep = []
            before_for_key = len(rseqs)
            deleting = 0
            for rseq in rseqs:
                if not self.try_match_rsyn_acked_to_tp_no_ack(rseq, key):
                    if not self.try_match_rsyn_acked_to_tp(rseq, key):
                        keep.append(rseq)
                    else:
                        deleting += 1
                else:
                    deleting += 1
            if len(keep) == 0:
                now_empty_keys.append(key)
            else:
                self.regular_acked[key] = keep
                after_for_key = len(keep)
                assert (
                    after_for_key + deleting
                ) == before_for_key, "deletion didn't work"
                total_deleting += deleting
        for key in now_empty_keys:
            total_deleting += len(self.regular_acked[key])
            del self.regular_acked[key]
        after = count(self.regular_acked)
        assert (after + total_deleting) == before, "Couldn't delete all rseqs"
        print(f"[PHASE] matched {before - after} reg. seq. to an earlier first phase")

    # -- evict and write ------------------------------------------------------

    def evict(self, batch_id):
        # Collect everything we want to write to a file.
        elems = []

        # Write irregular_syns
        # print("[PHASE] filtering irregular syns")
        before = count(self.irregular_syns)
        tmp = filter_list(self.irregular_syns, batch_id, "isyn")
        remaining = count(self.irregular_syns)
        print(f"[PHASE] evicting {len(tmp)} isyns")
        print(f"[PHASE]  remaining: {remaining}")
        after = count(self.irregular_syns)
        assert (after + len(tmp)) == before, "isyn deletion failed."
        elems.extend(tmp)

        # Write regular_syns
        # print("[PHASE] filtering regular syns")
        before = count(self.regular_syns)
        tmp = filter_list(self.regular_syns, batch_id, "rsyn")
        remaining = count(self.regular_syns)
        print(f"[PHASE] evicting {len(tmp)} rsyns")
        print(f"[PHASE]  remaining: {remaining}")
        after = count(self.regular_syns)
        assert (after + len(tmp)) == before, "rsyn deletion failed."
        elems.extend(tmp)

        # Cleanup acks (no writing).
        before = count(self.acks)
        tmp = filter_list(self.acks, batch_id, "ack")
        remaining = count(self.acks)
        print(f"[PHASE] evicting {len(tmp)} acks")
        print(f"[PHASE]  remaining: {remaining}")
        after = count(self.acks)
        assert (after + len(tmp)) == before, "ack deletion failed."
        tmp.clear()

        # Write irregular_acked
        # print("[PHASE] filtering irregular acked")
        before = count(self.irregular_acked)
        tmp = filter_dict_elems(self.irregular_acked, batch_id, "isyn (acked)", "isyn")
        remaining = count(self.irregular_acked)
        print(f"[PHASE] evicting {len(tmp)} acked isyns")
        print(f"[PHASE]  remaining: {remaining}")
        after = count(self.irregular_acked)
        assert (after + len(tmp)) == before, "iacked deletion failed."
        elems.extend(tmp)

        # Write regular_acked
        # print("[PHASE] filtering regular acked")
        before = count(self.regular_acked)
        tmp = filter_dict_elems(self.regular_acked, batch_id, "rsyn (acked)", "rsyn")
        remaining = count(self.regular_acked)
        print(f"[PHASE] evicting {len(tmp)} acked rsyns")
        print(f"[PHASE]  remaining: {remaining}")
        after = count(self.regular_acked)
        assert (after + len(tmp)) == before, "racked deletion failed."
        elems.extend(tmp)

        # Write two_phase_no_ack
        # print("[PHASE] filtering two phase no ack")
        before = count(self.two_phase_no_ack)
        tmp = filter_dict_elems(
            self.two_phase_no_ack, batch_id, "two-phase (no ack)", "isyn"
        )
        remaining = count(self.two_phase_no_ack)
        print(f"[PHASE] evicting {len(tmp)} two phase events without ack")
        print(f"[PHASE]  remaining: {remaining}")
        after = count(self.two_phase_no_ack)
        assert (after + len(tmp)) == before, "tp-no-ack deletion failed."
        elems.extend(tmp)

        # Write two_phase
        # print("[PHASE] filtering two phase")
        before = count(self.two_phase)
        tmp = filter_dict_elems(self.two_phase, batch_id, "two-phase", "isyn")
        remaining = count(self.two_phase)
        print(f"[PHASE] evicting {len(tmp)} two phase events with ack")
        print(f"[PHASE]  remaining: {remaining}")
        after = count(self.two_phase)
        assert (after + len(tmp)) == before, "tp deletion failed."
        elems.extend(tmp)

        # Repeated connection attempts
        self.repeated_connection_attempts.clear()
        err_msg = "failed to clear repeated connection apptempts"
        assert len(self.repeated_connection_attempts) == 0, err_msg

        print(f"[PHASE] cleaned up {len(elems)} elements")
        print("[PHASE] sorting them ...")
        elems.sort(key=lambda x: x["ts"])

        return elems

    def evict_and_write_matches(self, datasource, batch_id, file_timestamp):
        print(f"[PHASE] evicting batches with id {batch_id}")
        fn = f"{datasource}-events-{file_timestamp}.json.gz"
        print(f"[PHASE] writing {fn}")
        with gzip.open(fn, "wt", newline="") as fh:
            elems = self.evict(batch_id)
            print("[PHASE] writing to file")
            for elem in elems:
                json.dump(elem, fh)
                fh.write("\n")
            print("[PHASE] ok")

    # -- retransmits ----------------------------------------------------------

    def is_retransmit(self, event):
        snum = event.packet.payload.snum
        sa, da, sp, dp = event.packet.tuple()
        batch_id = event.batch_id
        tup = (sa, da, sp, dp, snum)
        # Check current batch.
        if (batch_id in self.retransmit_cache) and (
            tup in self.retransmit_cache[batch_id]
        ):
            return True
        elif (
            (batch_id > 0)
            and ((batch_id - 1) in self.retransmit_cache)
            and (tup in self.retransmit_cache[batch_id - 1])
        ):
            # Always collect in current batch.
            self.retransmit_cache[batch_id].add(tup)
            return True
        else:
            self.retransmit_cache[batch_id].add(tup)
            return False

        if False:
            tup = event.packet.tuple()
            batch_id = event.batch_id
            # Check last batch.
            if batch_id > 0 and (batch_id - 1) in self.retransmit_cache:
                prev_bid = batch_id - 1
                if (
                    tup in self.retransmit_cache[prev_bid]
                    and snum == self.retransmit_cache[prev_bid][tup]
                ):
                    # Always collect in current batch.
                    self.retransmit_cache[batch_id][tup] = snum
                    return True
            # Check current batch.
            if (
                batch_id in self.retransmit_cache
                and tup in self.retransmit_cache[batch_id]
                and snum == self.retransmit_cache[batch_id][tup]
            ):
                return True
            else:
                self.retransmit_cache[batch_id][tup] = snum
            return False

    def evict_retransmits(self, batch_id):
        if batch_id in self.retransmit_cache:
            del self.retransmit_cache[batch_id]
        print(f"[EVICT] retransmits in cache: {count(self.retransmit_cache)}")
        self.retransmits.clear()
        assert len(self.retransmits) == 0, "failed to evict retransmits"

    # -- matching -------------------------------------------------------------

    def try_match_rsyn_to_isyn(self, rsyn, key):
        if key not in self.irregular_syns:
            return False
        ts_syn = get_unix_timestamp(rsyn.packet.timestamp)
        repertory = self.irregular_syns[key]
        matching_index = None
        for i, isyn in enumerate(repertory):
            if is_within_timeout(isyn.probe_confirmation, ts_syn, self.phase_timeout_s):
                matching_index = i
                d = {
                    "isyn": isyn,
                    "iack": None,
                    "rsyn": rsyn,
                    "rack": None,
                }
                self.two_phase_no_ack[key].append(d)
                break
        if matching_index is None:
            return False
        before = len(self.irregular_syns[key])
        del repertory[matching_index]
        after = len(self.irregular_syns[key])
        assert before == after + 1, "isyn deletion failed"
        return True

    def try_match_rsyn_to_isyn_acked(self, rsyn, key):
        if key not in self.irregular_acked:
            return False
        ts_syn = get_unix_timestamp(rsyn.packet.timestamp)
        repertory = self.irregular_acked[key]
        matching_index = None
        for i, p1 in enumerate(repertory):
            if is_within_timeout(
                p1["isyn"].probe_confirmation, ts_syn, self.phase_timeout_s
            ):
                matching_index = i
                p1["rsyn"] = rsyn
                p1["rack"] = None
                self.two_phase_no_ack[key].append(p1)
                break
        if matching_index is None:
            return False
        before = len(self.irregular_acked[key])
        del repertory[matching_index]
        after = len(self.irregular_acked[key])
        assert before == after + 1, "isyn deletion failed"
        return True

    def try_match_rsyn_acked_to_isyn(self, p2, key):
        if key not in self.irregular_syns:
            return False
        ts_syn = get_unix_timestamp(p2["rsyn"].packet.timestamp)
        repertory = self.irregular_syns[key]
        matching_index = None
        for i, isyn in enumerate(repertory):
            if is_within_timeout(isyn.probe_confirmation, ts_syn, self.phase_timeout_s):
                matching_index = i
                p2["isyn"] = isyn
                p2["sack"] = None
                self.two_phase[key].append(p2)
                break
        if matching_index is None:
            return False
        before = len(self.irregular_syns[key])
        del repertory[matching_index]
        after = len(self.irregular_syns[key])
        assert before == after + 1, "isyn deletion failed"
        return True

    def try_match_rsyn_acked_to_isyn_acked(self, p2, key):
        if key not in self.irregular_acked:
            return False
        ts_syn = get_unix_timestamp(p2["rsyn"].packet.timestamp)
        repertory = self.irregular_acked[key]
        matching_index = None
        for i, p1 in enumerate(repertory):
            if is_within_timeout(
                p1["isyn"].probe_confirmation, ts_syn, self.phase_timeout_s
            ):
                matching_index = i
                p1.update(p2)
                self.two_phase[key].append(p1)
                break
        if matching_index is None:
            return False
        before = len(self.irregular_acked[key])
        del repertory[matching_index]
        after = len(self.irregular_acked[key])
        assert before == after + 1, "isyn deletion failed"
        return True

    def try_match_ack_to_isyn(self, ack, key):
        if key not in self.irregular_syns:
            return False
        repertory = self.irregular_syns[key]
        matching_index = None
        for i, syn in enumerate(repertory):
            if is_matching_ack(syn.packet, syn.probe_request, ack.packet):
                #  TODO: Just add it, maybe sort by time?
                matching_index = i
                d = {
                    "isyn": syn,
                    "iack": ack,
                }
                self.irregular_acked[key].append(d)
                break
        if matching_index is None:
            return False
        before = len(self.irregular_syns[key])
        del repertory[matching_index]
        after = len(self.irregular_syns[key])
        assert before == after + 1, "isyn deletion failed"
        return True

    def try_match_ack_to_rsyn(self, ack, key):
        if key not in self.regular_syns:
            return False
        repertory = self.regular_syns[key]
        matching_index = None
        for i, syn in enumerate(repertory):
            if is_matching_ack(syn.packet, syn.probe_request, ack.packet):
                # TODO: Just add it, maybe sort by time?
                matching_index = i
                d = {
                    "rsyn": syn,
                    "rack": ack,
                }
                self.regular_acked[key].append(d)
                break
        if matching_index is None:
            return False
        before = len(self.regular_syns[key])
        del repertory[matching_index]
        after = len(self.regular_syns[key])
        assert before == after + 1, "rsyn deletion failed"
        return True

    def try_match_ack_to_tp(self, ack, key):
        if key not in self.two_phase_no_ack:
            return False
        # Only try to match ACK to second phase?
        repertory = self.two_phase_no_ack[key]
        matching_index = None
        for i, event in enumerate(repertory):
            rsyn = event["rsyn"]
            if is_matching_ack(rsyn.packet, rsyn.probe_request, ack.packet):
                matching_index = i
                event["rack"] = ack
                self.two_phase[key].append(event)
                break
        if matching_index is None:
            return False
        before = len(self.two_phase_no_ack[key])
        del repertory[matching_index]
        after = len(self.two_phase_no_ack[key])
        assert before == after + 1, "tp no-ack deletion failed"
        return True

    # -- repeated connections -------------------------------------------------

    def try_match_rsyn_acked_to_tp_no_ack(self, rseq, key):
        if key not in self.two_phase_no_ack:
            return False
        ts_rsyn = get_unix_timestamp(rseq["rsyn"].packet.timestamp)
        repertory = self.two_phase_no_ack[key]
        for ev in repertory:
            isyn = ev["isyn"]
            if is_within_timeout(
                isyn.probe_confirmation, ts_rsyn, self.phase_timeout_s
            ):
                rseq["ident"] = hash(
                    (
                        get_unix_timestamp(isyn.packet.timestamp),
                        isyn.packet.payload.sport,
                        isyn.packet.ipid,
                        isyn.packet.payload.anum,
                    )
                )
                self.repeated_connection_attempts[key].append(rseq)
                return True
        return False

    def try_match_rsyn_acked_to_tp(self, rseq, key):
        if key not in self.two_phase:
            return False
        ts_rsyn = get_unix_timestamp(rseq["rsyn"].packet.timestamp)
        repertory = self.two_phase[key]
        for ev in repertory:
            isyn = ev["isyn"]
            if is_within_timeout(
                isyn.probe_confirmation, ts_rsyn, self.phase_timeout_s
            ):
                rseq["ident"] = hash(
                    (
                        get_unix_timestamp(isyn.packet.timestamp),
                        isyn.packet.payload.sport,
                        isyn.packet.ipid,
                        isyn.packet.payload.anum,
                    )
                )
                self.repeated_connection_attempts[key].append(rseq)
                return True
        return False
