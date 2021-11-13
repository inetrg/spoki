#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Event types to handle the log results from spoki.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import pytz

from datetime import datetime, timedelta, timezone
from enum import Enum
from ipaddress import ip_address


from cse.types.protocols.tcp import TCP
from cse.types.protocols.udp import UDP
from cse.types.protocols.icmp import ICMP

import numpy as np


class Packet:
    def __init__(self, timestamp, saddr, daddr, ipid, ttl, tag, payload):
        self.timestamp = timestamp
        self.saddr = saddr
        self.daddr = daddr
        self.ipid = ipid
        self.ttl = ttl
        self.proto = tag  # String, either 'tcp', 'udp', or 'icmp'.
        self.payload = payload  # Should be ICMP, TCP, or UDP.
        self.batch_id = None  # Makes filtering a bit easier

    def __str__(self):
        return (
            "Packet(saddr={}, daddr={}, ttl={}, ipid={}, ts={}, proto={}, "
            "pl={})".format(
                self.saddr,
                self.daddr,
                self.ttl,
                self.ipid,
                self.timestamp,
                self.proto,
                self.payload,
            )
        )

    def key(self):
        if self.proto == "tcp" or self.proto == "udp":
            return (self.saddr, self.daddr, self.payload.dport)
        else:  # self.proto == 'icmp':
            return (self.saddr, self.daddr)

    def tuple(self):
        if self.proto == "tcp" or self.proto == "udp":
            return (self.saddr, self.daddr, self.payload.sport, self.payload.dport)
        else:  # self.proto == 'icmp':
            return (self.saddr, self.daddr)

    def set_batch_id(self, batch_id):
        self.batch_id = batch_id

    def to_dict(self):
        d = {
            "timestamp": int(self.timestamp.timestamp() * 1000),
            "saddr": str(self.saddr),
            "daddr": str(self.daddr),
            "ipid": int(self.ipid),
            "ttl": int(self.ttl),
            "proto": self.proto,
            "payload": self.payload.to_dict(),
        }
        return d

    @staticmethod
    def from_dict(obj):
        sa = ip_address(obj["saddr"])
        da = ip_address(obj["daddr"])
        ttl = np.uint8(obj["ttl"])
        ipid = np.uint16(obj["ipid"])
        unix_ts = int(float(obj["observed"]) / 1000)
        raw_ts = datetime.fromtimestamp(unix_ts)
        ts = raw_ts.astimezone(timezone.utc)
        proto = ""
        payload = None
        if "tcp" in obj:
            proto = "tcp"
            payload = TCP.from_dict(obj["tcp"])
        elif "udp" in obj:
            proto = "udp"
            payload = UDP.from_dict(obj["udp"])
        elif "icmp" in obj:
            proto = "icmp"
            payload = ICMP.from_dict(obj["icmp"])
        return Packet(ts, sa, da, ipid, ttl, proto, payload)

    @staticmethod
    def from_csv(row):
        sa = ip_address(row["saddr"])
        da = ip_address(row["daddr"])
        ttl = np.uint8(row["ttl"])
        ipid = np.uint16(row["ipid"])
        unix_ts = int(float(row["ts"]) / 1000)
        raw_ts = datetime.fromtimestamp(unix_ts)
        ts = raw_ts.astimezone(timezone.utc)
        proto = row["proto"]
        payload = None
        if proto == "tcp":
            payload = TCP.from_csv(row)
        elif proto == "udp":
            payload = UDP.from_csv(row)
        elif proto == "icmp":
            payload = ICMP.from_csv(row)
        return Packet(ts, sa, da, ipid, ttl, proto, payload)
