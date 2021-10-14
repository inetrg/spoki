#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
A probe request issued by spoki to scamper.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import json

import numpy as np

from enum import Enum
from ipaddress import ip_address


class ProbeRequest:
    def __init__(
        self,
        saddr,
        daddr,
        sport,
        dport,
        anum,
        snum,
        userid,
        method,
        num_probes,
        payload,
    ):
        self.saddr = saddr
        self.daddr = daddr
        self.sport = sport
        self.dport = dport
        self.anum = anum
        self.snum = snum
        self.userid = userid
        self.method = method
        self.num_probes = num_probes
        self.payload = payload
        self.batch_id = None  # Makes filtering a bit easier

    def set_batch_id(self, batch_id):
        self.batch_id = batch_id

    def to_dict(self):
        d = {}
        d["saddr"] = str(self.saddr)
        d["daddr"] = str(self.daddr)
        d["sport"] = int(self.sport)
        d["dport"] = int(self.dport)
        d["anum"] = int(self.anum)
        d["snum"] = int(self.snum)
        d["userid"] = int(self.userid)
        d["method"] = self.method
        d["num_probes"] = self.num_probes
        d["payload"] = self.payload
        return d

    def __str__(self):
        return json.dumps(self.to_dict())

    @staticmethod
    def from_dict(obj):
        sa = ip_address(obj["saddr"])
        da = ip_address(obj["daddr"])
        sp = np.uint16(obj["sport"])
        dp = np.uint16(obj["dport"])
        anum = np.uint32(obj["anum"])
        snum = np.uint32(obj["snum"])
        userid = np.uint32(obj["userid"])
        method = obj["method"]
        num = int(obj["num_probes"])
        pl = obj["payload"]
        return ProbeRequest(sa, da, sp, dp, anum, snum, userid, method, num, pl)

    @staticmethod
    def from_csv(row):
        # Change source and destination info!
        da = ip_address(row["saddr"])
        sa = ip_address(row["daddr"])
        dp = np.uint16(row["sport"])
        sp = np.uint16(row["dport"])
        # The rest if request specific in each row.
        anum = np.uint32(row["probe anum"])
        snum = np.uint32(row["probe snum"])
        userid = np.uint32(row["userid"])
        method = row["method"]
        num = int(row["num probes"])
        pl = ""
        return ProbeRequest(sa, da, sp, dp, anum, snum, userid, method, num, pl)
