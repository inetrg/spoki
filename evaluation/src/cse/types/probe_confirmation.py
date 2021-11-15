#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
A reply from the scamper daemon that reports on the probes sent for a
specific event.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import json

import numpy as np

from datetime import datetime, timedelta, timezone
from ipaddress import ip_address


class ProbeConfirmation:
    def __init__(
        self,
        sport,
        dport,
        src,
        dst,
        userid,
        method,
        payload,
        ping_sent,
        probe_size,
        timestamp,
        ttl,
        version,
    ):
        self.sport = sport
        self.dport = dport
        self.src = src
        self.dst = dst
        self.userid = userid
        self.method = method
        self.payload = payload
        self.ping_sent = ping_sent
        self.probe_size = probe_size
        self.timestamp = timestamp
        self.ttl = ttl
        self.version = version
        self.batch_id = None  # Makes filtering a bit easier

    def set_batch_id(self, batch_id):
        self.batch_id = batch_id

    @staticmethod
    def from_dict(obj):
        sport = np.uint16(obj["sport"])
        dport = np.uint16(obj["dport"])
        src = ip_address(obj["src"])
        dst = ip_address(obj["dst"])
        userid = np.uint32(obj["userid"])
        method = obj["method"]
        payload = obj["payload"]
        ping_sent = obj["ping_sent"]
        probe_size = obj["probe_size"]
        if "start" in obj:
            start_sec = obj["start"]["sec"]
            start_usec = obj["start"]["usec"]
            raw_start = datetime.fromtimestamp(start_sec)
            raw_start += timedelta(microseconds=start_usec)
        else:
            start = int(obj["timestamp"])
            raw_start = datetime.fromtimestamp(start)
        start = raw_start.astimezone(timezone.utc)
        ttl = np.uint8(obj["ttl"])
        version = obj["version"]
        return ProbeConfirmation(
            sport,
            dport,
            src,
            dst,
            userid,
            method,
            payload,
            ping_sent,
            probe_size,
            start,
            ttl,
            version,
        )

    @staticmethod
    def from_csv(row):
        sport = np.uint16(int(row["sport"]))
        dport = np.uint16(int(row["dport"]))
        src = ip_address(row["saddr"])
        dst = ip_address(row["daddr"])
        userid = np.uint32(row["userid"])
        method = row["method"]
        payload = ""
        ping_sent = row["num probes"]
        probe_size = 0
        raw_start = None
        if "start sec" in row and "start usec" in row:
            start_sec = int(row["start sec"])
            start_usec = int(row["start usec"])
            raw_start = datetime.fromtimestamp(start_sec)
            raw_start += timedelta(microseconds=start_usec)
        else:
            timestamp = int(row["timestamp"])
            raw_start = datetime.fromtimestamp(timestamp)
        start = raw_start.astimezone(timezone.utc)
        ttl = 0
        version = 0
        return ProbeConfirmation(
            sport,
            dport,
            src,
            dst,
            userid,
            method,
            payload,
            ping_sent,
            probe_size,
            start,
            ttl,
            version,
        )

    def to_dict(self):
        d = {}
        d["sport"] = int(self.sport)
        d["dport"] = int(self.dport)
        d["src"] = str(self.src)
        d["dst"] = str(self.dst)
        d["userid"] = int(self.userid)
        d["method"] = str(self.method)
        d["payload"] = str(self.payload)
        d["ping_sent"] = int(self.ping_sent)
        d["probe_size"] = int(self.probe_size)
        d["timestamp"] = int(self.timestamp.timestamp() * 1000)
        d["ttl"] = int(self.ttl)
        d["version"] = str(self.version)
        return d

    def __str__(self):
        return json.dumps(self.to_dict())
