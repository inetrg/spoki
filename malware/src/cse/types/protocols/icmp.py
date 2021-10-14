#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
ICMP data for spoki events.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import json


class ICMP:
    def __init__(self, method, payload):
        self.method = method
        self.payload = payload

    def to_dict(self):
        d = {"method": self.method, "payload": self.payload}
        return d

    @staticmethod
    def from_dict(obj):
        m = obj["type"]
        pl = "empty"
        if "unreachable" in obj:
            pl = obj["unreachable"]
        return ICMP(m, pl)

    @staticmethod
    def from_csv(row):
        m = row["options"]
        pl = ""
        return ICMP(m, pl)

    def __str__(self):
        return json.dumps(self.to_dict())
