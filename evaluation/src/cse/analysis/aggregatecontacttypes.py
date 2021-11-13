#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Read one or more contacttype files and evaluate their combined data. Print numbers for each contacttype.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import click
import gzip
import json
import mmap
import os
import sys

import pandas as pd

csvheader = [
    "all",
    "without-ack",
    "without-payload",
    "with-payload",
    "ascii",
    "hex",
    "downloader",
    "matched",
]


def bufcount(filename):
    f = gzip.open(filename, "rt")
    lines = 0
    buf_size = 1024 * 1024
    read_f = f.read  # loop optimization
    buf = read_f(buf_size)
    while buf:
        lines += buf.count("\n")
        buf = read_f(buf_size)
    return lines


def get_num_lines(file_path):
    fp = gzip.open(file_path, "rt+")
    buf = mmap.mmap(fp.fileno(), 0)
    lines = 0
    while buf.readline():
        lines += 1
    return lines


def get_data(ev):
    tr = ev["trigger"]
    if "tcp" in tr:
        tcp = tr["tcp"]
        syn = tcp["syn"]
        ack = tcp["ack"]
        # fin = tcp["fin"]
        # rst = tcp["rst"]
        if ack and not syn:
            # IP data.
            saddr = tr["saddr"]
            daddr = tr["daddr"]
            # TCP data.
            sport = int(tcp["sport"])
            dport = int(tcp["dport"])
            snum = int(tcp["snum"])
            payload = tcp["payload"]
            if payload is not None and payload != "":
                return (saddr, daddr, sport, dport, snum, payload)
    return None


@click.command()
@click.argument(
    "files",
    nargs=-1,
    type=str,
)
def main(files):
    df = pd.concat([pd.read_csv(f, sep="|") for f in files], ignore_index=True)
    print(f"read {len(df)} lines of data")
    print(df)

    all = df["all"].sum()
    woack = df["without-ack"].sum()
    wopl = df["without-payload"].sum()
    wpl = df["with-payload"].sum()
    ascii = df["ascii"].sum()
    hex = df["hex"].sum()
    dl = df["downloader"].sum()
    matched = df["matched"].sum()

    check_payload_numbers = ascii + hex + dl
    print(
        f"payloads: {wpl} == sum: {check_payload_numbers} --> {check_payload_numbers == wpl}"
    )

    check_two_phase_events = woack + wopl + wpl
    print(
        f"two-phase events: {all} == sum: {check_two_phase_events} --> {check_two_phase_events == all}"
    )

    def pct(x):
        return round((x * 100 / all), 2)

    print(f"two-phase events = {all:>10}, {pct(all):>6.2f}%")
    print(f" without ack     = {woack:>10}, {pct(woack):>6.2f}%")
    print(f" without payload = {wopl:>10}, {pct(wopl):>6.2f}%")
    print(f" with payload    = {wpl:>10}, {pct(wpl):>6.2f}%")
    print(f"  ascii          = {ascii:>10}, {pct(ascii):>6.2f}%")
    print(f"  hex            = {hex:>10}, {pct(hex):>6.2f}%")
    print(f"  downloader     = {dl:>10}, {pct(dl):>6.2f}%")
    print(f"matched          = {matched:>10}, {pct(matched):>6.2f}%")


if __name__ == "__main__":
    main()
