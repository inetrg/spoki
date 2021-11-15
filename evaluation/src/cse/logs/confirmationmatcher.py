#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
These classes reads JSON data from a variety sources and parse them into
objects that support a `from_dict` method.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

from collections import defaultdict


# -- count --------------------------------------------------------------------


def count(collection):
    cnt = 0
    for val in collection.values():
        cnt += len(val)
    return cnt


# -- match requests and responses ---------------------------------------------


def req_key(req):
    return req.saddr, req.daddr, req.sport, req.dport, req.method, req.userid


def con_key(con):
    return con.src, con.dst, con.sport, con.dport, con.method, con.userid


def is_match(req, con):
    return req_key(req) == con_key(con)


# -- match probe requests and confirmations -----------------------------------


class ConfirmationMatcher:
    def __init__(self, confirmation_reader, event_reader, probe_timeout):
        self.cr = confirmation_reader
        self.er = event_reader

        self.ccache = defaultdict(lambda: [])
        self.ecache = []

        # Keep track of elements in cache so we don't have to count each time.
        self.ccnt = 0

        # Timestamps of newest entry.
        self.cts = None
        self.ets = None

        self.not_probed = []
        self.matched = []

        self.probe_timeout = probe_timeout

        # Readers already initialized the first file and incremented the
        # batch id accordingly. The first one will be (current id - 1).
        self.batch_id_order = [self.cr.batch_id - 1, self.cr.batch_id]
        self.observed_batch_ids = set(self.batch_id_order)

    # -- confirmations --------------------------------------------------------

    def load_confirmations(self):
        confirmations = self.cr.next_batch()
        for con in confirmations:
            self.ccache[con.userid].append(con)
            if self.cts is None or self.cts < con.timestamp:
                self.cts = con.timestamp
        self.ccnt += len(confirmations)
        print(f"[LOAD] loaded {len(confirmations)} confirmations")

    def evict_confirmations(self, batch_id):
        evict_cnt = 0
        unsure_cnt = 0
        empty_keys = []

        for key, confirmations in self.ccache.items():
            before = len(confirmations)
            unsure = [
                c
                for c in confirmations
                if c.batch_id == batch_id and c.method != "tcp-rst"
            ]
            confirmations = [c for c in confirmations if c.batch_id != batch_id]
            unsure_cnt += len(unsure)
            # for elem in unsure:
            # print(f" > {json.dumps(elem.to_dict())}")
            removed_elements = before - len(confirmations)
            evict_cnt += removed_elements
            self.ccnt -= removed_elements
            self.ccache[key] = confirmations
            if len(confirmations) == 0:
                empty_keys.append(key)
        for key in empty_keys:
            del self.ccache[key]
        print(f"[EVICT] evicted {evict_cnt} elements (usure: {unsure_cnt})")

    def find_confirmation(self, pkt_ts, req):
        uid = req.userid
        confirmations = self.ccache[uid]
        start = len(self.ccache[uid])
        match = None
        for i, con in enumerate(confirmations):
            if is_match(req, con):
                con_ts = con.timestamp
                diff = con_ts - pkt_ts if con_ts > pkt_ts else pkt_ts - con_ts
                if diff <= self.probe_timeout:
                    match = con
                    before = len(self.ccache[uid])
                    assert start == before, "uhm?"
                    del confirmations[i]
                    after = len(self.ccache[uid])
                    assert before == after + 1, "con cache deletion failed"
                    assert match is not None, "should not return 'None'"
                    self.ccnt -= 1
                    break
        if len(confirmations) == 0:
            del self.ccache[uid]
        return match

    def confirmations_cached(self):
        # Probably be more efficient to count this live. Used this code to
        # check that it is correct.
        # fresh = count(self.ccache)
        # err_msg = f"confirmation count wrong: {self.ccnt} != {fresh}"
        # assert self.ccnt == fresh, err_msg
        return self.ccnt

    def last_confirmation_timestamp(self):
        return self.cts

    # -- events ---------------------------------------------------------------

    def load_events(self):
        before = len(self.ecache)
        batch = self.er.next_batch()
        for event in batch:
            if self.ets is None or self.ets < event.packet.timestamp:
                self.ets = event.packet.timestamp
        self.ecache.extend(batch)
        after = len(self.ecache)
        print(f"[LOAD] loaded {after - before} events")

    def evict_events(self, batch_id):
        before = len(self.ecache)
        keeping = [e for e in self.ecache if e.batch_id != batch_id]
        after = len(keeping)
        discarding = before - after
        if discarding > 0:
            print(f"[EVICT] !!! discarding {discarding} old events")
        self.ecache = keeping

    def match_events(self):
        print("[MATCH] matching confirmations to events")
        # matches = []
        # unprobed = []
        results = []
        unmatched = []

        # Go through all cached events and find confirmations.
        events_with_confirmation = 0
        events_without_probe = 0
        ids_in_batch = set()
        for event in self.ecache:
            ids_in_batch.add(event.batch_id)
            # If we didn't probe it pass it on.
            if event.probe_request is not None:
                pkt = event.packet
                req = event.probe_request
                con = self.find_confirmation(pkt.timestamp, req)
                if con is not None:
                    event.probe_confirmation = con
                    # matches.append(event)
                    results.append(event)
                    events_with_confirmation += 1
                else:
                    # Maybe we will find the event in the next batch.
                    unmatched.append(event)
            else:
                # Just pass these on.
                # unprobed.append(event)
                events_without_probe += 1
                results.append(event)
        self.ecache = unmatched

        print(f"[MATCH] encountered ids: {ids_in_batch}")
        self.observed_batch_ids.update(ids_in_batch)
        # Evict old data if we reached the next next hour.
        if len(self.observed_batch_ids) >= 3:
            print("[EVICT] evicting old batch")
            # Find oldest batch IDs, which is the one we want to evict.
            oldest_id = self.batch_id_order[0]
            print(f"[EVICT] oldest id is {oldest_id} of {self.batch_id_order}")
            # Evict everything.
            self.evict_confirmations(oldest_id)
            self.evict_events(oldest_id)
            # Setup batch id tracking for next "rollover".
            previous_ids = set(self.batch_id_order)
            new_ids = list(self.observed_batch_ids.difference(previous_ids))
            if len(new_ids) > 1:
                print(f"[EVICT] !!! unexpected number of new ids: {new_ids}")
            self.batch_id_order.extend(new_ids)
            print(f"[EVICT] before discard: {self.observed_batch_ids}")
            self.observed_batch_ids.discard(oldest_id)
            del self.batch_id_order[0]
            print(f"[EVICT] new observed id set: {self.observed_batch_ids}")
            print(f"[EVICT] with ordering {self.batch_id_order}")

        print(f"[MATCH] matched {events_with_confirmation} event with a conf.")
        print(f"[MATCH] found {events_without_probe} events that we didn't react to.")
        print(f"[MATCH] now got {len(self.ecache)} events remaining in cache.")
        # return matches, unprobed
        return results

    def events_cached(self):
        return len(self.ecache)

    def last_event_timestamp(self):
        return self.ets
