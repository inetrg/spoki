#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Analyze a packet ... a tiny bit
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

from cse.types.packet import Packet


class Analyzer:
    def __init__(self, pkt):
        self.pkt = pkt

    def is_syn(self):
        """
        Check if the packet contains TCP and is a TCP SYN packet.
        """
        pl = self.pkt.payload
        if self.pkt.proto == "tcp" and "syn" in pl.flags and "ack" not in pl.flags:
            return True
        return False

    def is_ack(self):
        """
        Check if the packet contains TCP and is a TCP ACK packet.
        """
        pl = self.pkt.payload
        if self.pkt.proto == "tcp" and "ack" in pl.flags and "syn" not in pl.flags:
            return True
        return False

    def is_syn_ack(self):
        """
        Check if the packet contains TCP and is a TCP SYN ACK packet.
        """
        pl = self.pkt.payload
        if self.pkt.proto == "tcp" and "syn" in pl.flags and "ack" in pl.flags:
            return True
        return False

    def is_fin_ack(self):
        """
        Check if the packet contains TCP and is a TCP SYN ACK packet.
        """
        pl = self.pkt.payload
        if self.pkt.proto == "tcp" and "ack" in pl.flags and "fin" in pl.flags:
            return True
        return False

    def is_rst(self):
        """
        Check if the packet contains TCP and is a TCP RST packet.
        """
        pl = self.pkt.payload
        if self.pkt.proto == "tcp" and "rst" in pl.flags:
            return True
        return False

    def is_irregular_syn(self):
        """
        Check if a Packet containing TCP might be a two phase scan
        """
        if not self.is_syn():
            return False
        if (
            self.pkt.ipid == 54321
            or self.pkt.ttl > 200
            or (len(self.pkt.payload.options) == 0)
        ):
            return True
        return False

    def is_scanner_syn(self):
        """
        Renamed, here for legacy stuff ...
        """
        return self.is_irregular_syn()

    def scanner_tool(self):
        """
        Try to figure out the scanner tool that send the probe. Based on
        "Remote Identification of Port Scan Toolchains", Ghiette et al.
        """
        if self.is_syn():
            # -- zmap -----
            if self.pkt.ipid == 54321:
                # see https://github.com/zmap/zmap
                # in src/probe_modules/packet.c:89
                return "zmap"
            # -- massan ---
            pl = self.pkt.payload
            masscan_ipid = pl.dport ^ pl.snum ^ int(self.pkt.daddr)
            if self.pkt.ipid == masscan_ipid:
                return "masscan"
            # -- unicorn -- (not implemented)
            #  * uses a 32 bit session key and then sets seq to
            #   key ^ saddr ^ ((sport << 16) + dport)
            #  * key should be the same for probes from the same instance
            # return 'unicorn'
            # -- nmap ----- (not implemented)
            #  * has a key as well and multiple probes could be attributed to
            #   the same instance as well
            # return 'nmap'
            # -- mirai ----
            if (pl.dport == 23 or pl.dport == 2323) and pl.snum == int(self.pkt.daddr):
                # see https://github.com/jgamblin/Mirai-Source-Code
                # in mirai/bot/scanner.c:225
                return "mirai"
        return "unknown"
