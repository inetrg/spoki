#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Ensure there were no payloads missed, then count contact events of scanners.
There is another script that reads the data and aggregates it. Since this
process can take a log of time on larger datasets it can be run in parallel
with different fileids (file acces is NOT thread safe). The created files can
them be fed into the contacttype script to get the aggregated results.

Example:
time contact-types -r data -i 1 assembled/test-events-20200622-230000.json.gz
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import click
import csv
import gzip
import json
import mmap
import os

from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from tqdm import tqdm

from cse.payloads.analyzer import PayloadAnalyzer


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
    # ts|saddr|daddr|ipid|ttl|proto|sport|dport|anum|snum|options|payload
    #  |syn|ack|rst|fin|window size|probed|method|userid|probe anum
    #  |probe snum|num probes
    proto = ev['proto']
    if proto == 'tcp':
        syn = int(ev["syn"])
        ack = int(ev["ack"])
        # fin = tcp["fin"]
        # rst = tcp["rst"]
        if ack == 1 and syn == 0:
            # IP data.
            saddr = ev["saddr"]
            daddr = ev["daddr"]
            # TCP data.
            sport = int(ev["sport"])
            dport = int(ev["dport"])
            snum = int(ev["snum"])
            payload = ev["payload"]
            if payload is not None and payload != "":
                return (saddr, daddr, sport, dport, snum, payload)
    return None


@click.command()
@click.argument(
    "logfile",
    type=click.Path(exists=True),
)
@click.option(
    "--raw",
    "-r",
    "rawdir",
    # type=click.Path(exists=True),
    type=str,
    default="data",
    help="directory with raw logs",
)
@click.option(
    "--compressed",
    is_flag=True,
    help="load compressed files",
)
@click.option(
    "--id",
    "-i",
    "fileid",
    required=True,
    type=int,
    help="give the log file a unique name",
)
def main(logfile, rawdir, compressed, fileid):
    # Calculate raw file name from logfile.
    fn = logfile
    fn = fn.split("/")[-1]
    fn = fn.replace(".json.gz", "")
    # print(f"no ending = {fn}")
    parts = fn.split("-events-")
    vantagepoint = parts[0]
    # print(f"vantage point = {vantagepoint}")
    parts = parts[1].split("-")
    date = parts[0]
    # print(f"date = {date}")
    year = date[0:4]
    month = date[4:6]
    day = date[6:8]
    # print(f"year = {year}, month = {month}, day = {day}")
    time = parts[1]
    # print(f"time = {time}")
    hour = time[:2]
    # print(f"hour = {hour}")

    dateobj = datetime(
        int(year),
        int(month),
        int(day),
        int(hour),
        tzinfo=timezone.utc,
    )
    # print(f"as date: {dateobj}")

    unixts = int(dateobj.astimezone(timezone.utc).timestamp())
    # print(f"as unixtime: {unixts}")

    # Build ACK DB
    # Maybe (src, dst, sport, dport) -> [(snum, pl), ..]
    ackdb = defaultdict(lambda: [])
    
    # Sample filename: 2021-10-13.22:00:00.test.spoki.tcp.raw.1634162400.csv
    rawlog = f"{year}-{month}-{day}.{hour}:00:00.{vantagepoint}.spoki.tcp.raw.{unixts}.csv"
    if compressed:
        rawlog += '.gz'
    # print(f"related log file: {rawlog}")

    filepath = os.path.join(rawdir, rawlog)

    if not Path(filepath).is_file():
        print(f"can't find related raw log file: '{rawlog}' in '{rawdir}'")
        return

    def read_file(fh):
        nonlocal ackdb
        reader = csv.DictReader(fh, delimiter='|')
        for row in reader:
            attribues = get_data(row)
            if attribues is not None:
                saddr, daddr, sport, dport, snum, payload = attribues
                ackdb[(saddr, daddr, sport, dport)].append((snum, payload))

    if compressed:
        with gzip.open(filepath, "rt") as fh:
            read_file(fh)
                
    else:
        with open(filepath, "rt") as fh:
            read_file(fh)
    # print(f"got alternative acks for {len(ackdb)} addresses")

    # Collect stats.
    # - number of two phase events
    #   - all
    #     - without ack
    #     - without payload
    #     - with payload
    #       - ascii
    #       - hex
    #       - downloader
    all = 0
    with_payload = 0
    ascii_payload = 0
    hex_payload = 0
    downloader = 0
    without_payload = 0
    without_ack = 0
    found_matching_payload = 0

    plana = PayloadAnalyzer()

    def count_payload_type(pl):
        nonlocal with_payload
        nonlocal ascii_payload
        nonlocal hex_payload
        nonlocal downloader
        decoded, success = plana.try_decode_hex(pl)
        if success:
            res = plana.extract_url(decoded)
            if res is not None:
                downloader += 1
            else:
                ascii_payload += 1
        else:
            hex_payload += 1

    # - Read assembled log. Find two-phase events.
    # - If event has payload: continue
    # - If event has no payload: check ACK DB.
    two_phase_tags = set(["two-phase (no ack)", "two-phase"])
    tags_to_skip = set(["isyn (acked)", "isyn", "rsyn (acked)", "rsyn", "ack"])
    lines = bufcount(logfile)
    tup_set = set()
    tup_list = []
    with gzip.open(logfile, "rt") as fh:
        # for line in fh:
        for line in tqdm(fh, total=lines):
            try:
                ev = json.loads(line)
                tag = ev["tag"]
                # We only want the two-phase events.
                if tag in two_phase_tags:
                    all += 1
                    # Was there ever an ack?
                    if "rack" in ev and ev["rack"] is not None:
                        rack = ev["rack"]
                        pl = rack["trigger"]["payload"]["payload"]
                        if pl != "":
                            with_payload += 1
                            count_payload_type(pl)
                        else:
                            pkt = ev["rsyn"]
                            saddr = pkt["trigger"]["saddr"]
                            daddr = pkt["trigger"]["daddr"]
                            sport = int(pkt["trigger"]["payload"]["sport"])
                            dport = int(pkt["trigger"]["payload"]["dport"])
                            tup = (saddr, daddr, sport, dport)
                            # Check DB for alternative ack.
                            target_snum = rack["trigger"]["payload"]["snum"]
                            acks = ackdb[tup]
                            if len(acks) == 0:
                                without_payload += 1
                            else:
                                # print(f"found alternative ACKs")
                                # print(f"{saddr}, {daddr}, {sport}, {dport} - {target_snum}")
                                found_ack = False
                                for (snum, pl) in acks[:]:
                                    # print(f"  {snum}")
                                    if snum == target_snum:
                                        found_ack = True
                                        with_payload += 1
                                        found_matching_payload += 1
                                        count_payload_type(pl)
                                        ackdb[tup] = list(
                                            filter(
                                                lambda a: a[0] != target_snum,
                                                ackdb[tup],
                                            )
                                        )
                                        tup_set.add(tup)
                                        tup_list.append(tup)
                                        break
                                if not found_ack:
                                    without_payload += 1

                    else:
                        without_ack += 1
                elif tag in tags_to_skip:
                    pass
                else:
                    print(f"ERR: unexpected tag: {tag}")
            except Exception as e:
                print(f"ERR: failed to parse line '{line}': {e}")

    # print(f"found {found_matching_payload} payloads")
    # print(f"all = {all}")
    # print(f" without ack = {without_ack}")
    # print(f" without payload = {without_payload}")
    # print(f" with payload = {with_payload}")
    # print(f"  ascii = {ascii_payload}")
    # print(f"  hex = {hex_payload}")
    # print(f"  downloader = {downloader}")
    # print(f" tup_set = {len(tup_set)}, tup_list = {len(tup_list)}")

    # Write JSON or CSV log.
    row = [
        all,
        without_ack,
        without_payload,
        with_payload,
        ascii_payload,
        hex_payload,
        downloader,
        found_matching_payload,
    ]
    output = f"{vantagepoint}.contacttypes.{fileid}.csv.gz"
    if Path(output).is_file():
        with gzip.open(output, "at") as fh:
            writer = csv.writer(fh, delimiter="|")
            writer.writerow(row)
    else:
        # print("starting new file")
        with gzip.open(output, "wt") as fh:
            print(f"creating '{output}'")
            writer = csv.writer(fh, delimiter="|")
            writer.writerow(csvheader)
            writer.writerow(row)


if __name__ == "__main__":
    main()
