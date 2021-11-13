#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
TCP type for spoki events.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import json

import numpy as np


all_tcp_flags = [
    "fin",
    "syn",
    "rst",
    "psh",
    "ack",
    "urg",
    "ece",
    "cwr",
    "ns",
]


class TCP:
    def __init__(self, sport, dport, snum, anum, window_size, flags, options, payload):
        self.sport = sport
        self.dport = dport
        self.snum = snum
        self.anum = anum
        self.window_size = window_size
        self.flags = flags
        self.options = options
        self.payload = payload

    def to_dict(self):
        d = {}
        d["sport"] = int(self.sport)
        d["dport"] = int(self.dport)
        d["snum"] = int(self.snum)
        d["anum"] = int(self.anum)
        d["window_size"] = int(self.window_size)
        d["flags"] = self.flags
        d["options"] = self.options
        d["payload"] = self.payload
        return d

    @staticmethod
    def from_dict(obj):
        sp = np.uint16(obj["sport"])
        dp = np.uint16(obj["dport"])
        sn = np.uint32(obj["snum"])
        an = np.uint32(obj["anum"])
        ws = np.uint16(obj["window_size"])
        pl = obj["payload"]
        flags = []
        for flag in all_tcp_flags:
            # check if option exitsts
            if flag in obj:
                # check if it is true
                if obj[flag]:
                    flags.append(flag)
        opts = []
        if "options" in obj and obj["options"] is not None:
            opts = obj["options"]
            opts = list(opts.keys())
        return TCP(sp, dp, sn, an, ws, flags, opts, pl)

    @staticmethod
    def from_csv(row):
        sp = np.uint16(row["sport"])
        dp = np.uint16(row["dport"])
        sn = np.uint32(row["snum"])
        an = np.uint32(row["anum"])
        ws = np.uint16(row["window size"])
        pl = row["payload"]
        flags = []
        for flag in all_tcp_flags:
            # Check if option exists and is set.
            if flag in row and row[flag] == '1':
                flags.append(flag)
        opts = []
        if "options" in row and row["options"] != '':
            opts = list(row["options"].split(':'))
        return TCP(sp, dp, sn, an, ws, flags, opts, pl)

    def __str__(self):
        return json.dumps(self.to_dict())
