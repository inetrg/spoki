#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Event type that bundles a packet and the resulting probe request.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import json

from cse.types.packet import Packet
from cse.types.probe_request import ProbeRequest


class Event:
    def __init__(self, packet, probe_request):
        # The packet.
        self.packet = packet
        # The resulting probe request.
        self.probe_request = probe_request
        self.probe_confirmation = None
        self.batch_id = None  # Makes filtering a bit easier

    def to_dict(self):
        # Will keep the original log style and hence the names.
        d = {}
        d["trigger"] = self.packet.to_dict()
        if self.probe_request is not None:
            d["reaction"] = self.probe_request.to_dict()
        else:
            d["reaction"] = None
        return d

    def get_key(self):
        return self.packet.key()

    def set_batch_id(self, batch_id):
        self.batch_id = batch_id
        self.packet.set_batch_id(batch_id)
        if self.probe_request is not None:
            self.probe_request.set_batch_id(batch_id)

    def __str__(self):
        return json.dumps(self.to_dict())

    @staticmethod
    def from_dict(o):
        pkt = Packet.from_dict(o["trigger"])
        req = None
        if "reaction" in o and o["reaction"] is not None:
            req = ProbeRequest.from_dict(o["reaction"])
        return Event(pkt, req)

    @staticmethod
    def from_csv(row):
        pkt = Packet.from_csv(row)
        req = None
        if row["probed"] == "true":
            req = ProbeRequest.from_csv(row)
        return Event(pkt, req)
