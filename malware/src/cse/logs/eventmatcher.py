#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Events, i.e., received packets and their probe requests, are sorted and matched
together to identify two-phase events.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

from datetime import datetime, timedelta, timezone


# -- time --------------------------------------------------------------------


def make_file_timestr(timeobject):
    utc_time = timeobject.astimezone(timezone.utc)
    return utc_time.strftime("%Y%m%d-%H%M%S")


def file_timestr_from_event(ev):
    assert "ts" in ev, "no 'ts' key in event"
    unix_ts = ev["ts"]
    ts = datetime.fromtimestamp(unix_ts)
    return make_file_timestr(ts)


# -- class -------------------------------------------------------------------


class EventMatcher:
    def __init__(
        self,
        confirmation_matcher,
        phase_matcher,
        start_hour,
        writer,
        confirmation_load_threshold=timedelta(minutes=30),
    ):
        self.cm = confirmation_matcher
        self.pm = phase_matcher
        self.sh = start_hour
        self.wr = writer
        self.clt = confirmation_load_threshold

    def match(self):
        # Start dumping data 30 min past the end hour. Start hour should have
        # batch id 0.
        next_dump_ts = self.sh + timedelta(hours=1, minutes=30)
        batch_to_dump = 0
        ts_to_dump = self.sh

        events = []

        while True:
            # Don't load confirmations too far int he future.
            # TODO: Check timestamps instead numbers.
            # ecnt = confirmation_matcher.events_cached()
            # ccnt = confirmation_matcher.confirmations_cached()
            # if (ccnt - ecnt) < confirmation_load_threshold:
            #     confirmation_matcher.load_confirmations()
            ets = self.cm.last_event_timestamp()
            cts = self.cm.last_confirmation_timestamp()
            if ets is not None and cts is not None:
                ahead = "confirmations"
                if ets > cts:
                    ahead = "events"
                print(f"[TIME] delta is {abs(ets - cts)} ({ahead} are ahead)")

            # Load confirmations if they are not too far ahead.
            if cts is None or (cts - ets) < self.clt:
                self.cm.load_confirmations()
            else:
                print("[TIME] not loading confirmations")
            # Load events if they are not too far ahead.
            if ets is None or (ets - cts) < self.clt:
                self.cm.load_events()
            else:
                print("[TIME] not loading events")

            # Match them.
            new_events = self.cm.match_events()
            print(f"[ASSEM] {len(new_events)} new events")

            events.extend(new_events)
            keep_after = 0
            processed_events = 0

            # Pass collect events in long list ...
            for i, event in enumerate(events):
                processed_events += 1
                keep_after = i
                self.pm.add_event(event)
                # Reached next checkpoint?
                if event.packet.timestamp >= next_dump_ts:
                    assert keep_after == i
                    print("[ASSEM] reached checkpoint")
                    self.pm.try_match_everything()
                    self.pm.find_repeated_connections()
                    self.pm.evict_retransmits(batch_to_dump)
                    elems = self.pm.evict(batch_to_dump)
                    print(f"[ASSEM] evicting {len(elems)} elems")
                    if len(elems) == 0:
                        break
                    file_ts = file_timestr_from_event(elems[0])
                    self.wr.write_elems(elems, file_ts)
                    next_dump_ts += timedelta(hours=1)
                    ts_to_dump += timedelta(hours=1)
                    batch_to_dump += 1
                    break

            print(f"[ASSEM] got through {keep_after + 1} events")
            before = len(events)
            events = events[(keep_after + 1) :]
            after = len(events)
            err_msg = f"del error: {before} != {after} + {processed_events}"
            assert (before - after) == processed_events, err_msg
