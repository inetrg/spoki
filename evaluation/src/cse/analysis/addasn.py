#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Add ASN meta data to a gzipped CSV file with an IPv4 column. Based on pyasn module.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import argparse
import os
import pyasn

import pandas as pd

from ipaddress import ip_network
from pathlib import Path


# -- our purpose ---------------------------------------------------------------

def lookup_asn(db, ip):
    try:
        # Should return the origin AS, and the BGP prefix it matches
        asn, _ = db.lookup(ip)
        # if asn is None:
        #    print(' > could not find ASN for {}'.format(ip))
        return None if asn is None else int(asn)
    except:
        # print(f'WARN: failed to lookup asn for {ip}')
        return None


def lookup_prefix(db, ip):
    try:
        _, prefix = db.lookup(ip)
        return None if prefix is None else str(ip_network(prefix))
        # return str(ip_network(prefix))
    except:
        # print(f'WARN: failed to lookup prefix for {ip}')
        return None


def main():
    parser = argparse.ArgumentParser(description='Add ASN meta data to a CSV file.')
    parser.add_argument('-i', '--input', required=True, type=str,
                        help='file to annotate')
    parser.add_argument('-d', '--date', type=str, default='2020-05-15',
                        help='date of the database file (d: 2020-05-15)')
    parser.add_argument('-a', '--asndb-directory', type=str, default='ipasn',
                        help='directory to with ipasn database files (d: ipasn)')
    parser.add_argument('-f', '--field', type=str, action='append',
                        help='names of the columns to process')

    args = parser.parse_args()

    fn = args.input
    path = Path(fn)
    if not path.is_file():
        print('ERR: input is not a file')
        return

    if not Path(args.asndb_directory).is_dir():
        print('ERR: asndb directory is not a directory')
        return

    df = pd.read_csv(fn, sep="|")
    # print(df)

    print(f'-- loading ipasn database for {args.date}')
    asnfile = os.path.join(args.asndb_directory, f'ipasn_{args.date}.gz')
    asndb = pyasn.pyasn(asnfile)

    for field in args.field:
        print(f"field: {field}")
        pre_target = f'{field}.prefix'
        asn_target = f'{field}.asn'

        df[pre_target] = df[field].apply(lambda ip: lookup_prefix(asndb, ip))
        df[asn_target] = df[field].apply(lambda ip: lookup_asn(asndb, ip))

        total = len(df.index)
        pre_missing = df[pre_target].isnull().sum()
        asn_missing = df[asn_target].isnull().sum()

        print(f'total          = {total:>7}')
        print(
            f'missing prefix = {pre_missing:>7} ({round(pre_missing / total * 100, 2)}%)')
        print(
            f'missing ASN    = {asn_missing:>7} ({round(asn_missing / total * 100, 2)}%)')

    fn = fn.replace("csv.gz", "meta.csv.gz")
    print(f'writing "{fn}"')
    df.to_csv(fn, index=False, sep="|")


if __name__ == '__main__':
    main()
