#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Query Greynoise API for lists of IP addresses
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"


import csv
import click
import gzip
import os

from pathlib import Path

from cse.analysis.database.greynoise import GreyNoise

gn_key_env = "GN_API_KEY"


def write_to_file(lines, filename, header):
    if Path(filename).is_file():
        with gzip.open(filename, "at") as fh:
            writer = csv.DictWriter(fh, fieldnames=header, delimiter="|")
            writer.writerows(lines)
    else:
        print(f"opening {filename}")
        with gzip.open(filename, "wt") as fh:
            writer = csv.DictWriter(fh, fieldnames=header, delimiter="|")
            writer.writeheader()
            writer.writerows(lines)


@click.command()
@click.argument(
    "source_file",
    type=click.Path(exists=True),
)
@click.option(
    "--batch-size",
    "-b",
    "batch_size",
    default=500,
    type=int,
    help="set batch size, some calls don't support > 500 (500)",
)
@click.option(
    "--number-of-elements",
    "-n",
    "num",
    default=None,
    type=int,
    help="only process the first N elements (None -> all)",
)
def main(source_file, batch_size, num):
    if gn_key_env not in os.environ:
        print(f"[ERR] could not find Greynoise API key, please set {gn_key_env}")
        return

    gnkey = os.environ[gn_key_env]
    print(f"gn key is '{gnkey}'")

    output_file = "noised." + source_file

    gn = GreyNoise(gnkey)

    with gzip.open(source_file, "rt") as fh:
        reader = csv.DictReader(fh, delimiter="|")

        header = reader.fieldnames
        header.append("classification")
        header.append("spoofable")

        hits = 0
        no_entry = 0
        current_batch = []
        lines = {}
        total_rows_processed = 0

        def perform_lookup():
            nonlocal hits
            nonlocal no_entry
            nonlocal current_batch
            nonlocal lines

            res = gn.multi_context_query(current_batch)
            if "message" not in res:
                print(f"'message' field is not in res")
                print(res)
                print("early exit")
                exit()
            if res["message"] != "ok":
                print(f"unexpected result message: {res['message']}")
                print(res)
                print("early exit")
                exit()

            num_results = res["results"]
            hits += num_results
            no_entry += batch_size - num_results

            query_set = set(current_batch)
            for elem in res["data"]:
                ip = elem["ip"]
                query_set.remove(ip)
                lines[ip]["classification"] = elem["classification"]
                lines[ip]["spoofable"] = elem["spoofable"]
            for ip in query_set:
                lines[ip]["classification"] = "no result"
                lines[ip]["spoofable"] = "no result"

            write_to_file(list(lines.values()), output_file, header)

            lines = {}
            current_batch = []
            # print("early exit")
            # exit()

        num_width = 10
        if num is not None:
            num_width = len(str(num))

        collected = 0
        for line in reader:
            saddr = line["saddr"]
            lines[saddr] = line
            current_batch.append(saddr)
            collected += 1
            if collected >= batch_size:
                if num is not None:
                    print(f"> {total_rows_processed:>{num_width}} / {num:>{num_width}}")
                else:
                    print(f"> {total_rows_processed:>{num_width}} / *")
                total_rows_processed += collected
                perform_lookup()
                collected = 0
                if num is not None and total_rows_processed >= num:
                    print(
                        f"reached number of elements "
                        f"to process: {total_rows_processed} / {num}"
                    )
                    exit()

        perform_lookup()
