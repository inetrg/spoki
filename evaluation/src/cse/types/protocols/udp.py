#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
UDP data for events collected by spoki.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import json

import numpy as np


class UDP:
    def __init__(self, sport, dport, payload):
        self.sport = sport
        self.dport = dport
        self.payload = payload

    def to_dict(self):
        d = {}
        d["sport"] = int(self.sport)
        d["dport"] = int(self.dport)
        d["payload"] = self.payload
        return d

    @staticmethod
    def from_dict(obj):
        sp = np.uint16(obj["sport"])
        dp = np.uint16(obj["dport"])
        return UDP(sp, dp, obj["payload"])

    @staticmethod
    def from_csv(row):
        sp = np.uint16(row["sport"])
        dp = np.uint16(row["dport"])
        return UDP(sp, dp, row["payload"])

    def __str__(self):
        return json.dumps(self.to_dict())
