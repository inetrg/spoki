
#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Add meta data to zipped CSV file.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import argparse
import pyipmeta

import pandas as pd

from pathlib import Path


# addr.prefix|addr.asn|addr.country_code|addr.region_code|addr.continent_code|addr.city

# -- our purpose ---------------------------------------------------------------

# Cache ip meta data locally to ease the lookup of different fields
cache = {}


def add_to_cache(ipm, ip):
    global cache
    try:
        res = ipm.lookup(ip)
        if len(res) > 0:
            cache[ip] = res[0]
            return True
    except:
        pass
    print(f'WARN: failed to lookup meta data for {ip}')
    return False


def make_lookup(ipm, key):
    def lookup(ip):
        if ip in cache:
            return cache[ip][key]
        if add_to_cache(ipm, ip):
            return cache[ip][key]
        return None
    return lookup


def main():
    parser = argparse.ArgumentParser(description='Annotate CSV with geo data.')
    parser.add_argument('-i', '--input', required=True, type=str,
                        help='file to annotate')
    parser.add_argument('-d', '--date', type=str, default='2021-02-15',
                        help='date of the database file (d: 2021-02-15)')
    parser.add_argument('-f', '--field', type=str, action='append',
                        help='names of the columns to process')
    parser.add_argument('-p', '--provider', type=str, default='netacq-edge',
                        help='choose provider (d: netacq-edge)')

    args = parser.parse_args()

    fn = args.input
    path = Path(fn)
    if not path.is_file():
        print('ERR: input is not a file')
        return

    if len(args.field) < 1:
        print("EFF: need at least one field to process")
        return

    df = pd.read_csv(fn, sep="|")

    print('-- loading IP meta database for "{}"'.format(args.date))
    ipm = pyipmeta.IpMeta(provider=args.provider, time=args.date)

    for field in args.field:
        country = f'{field}.country_code'
        region = f'{field}.region_code'
        continent = f'{field}.continent_code'
        city = f'{field}.city'

        lu_country = make_lookup(ipm, 'country_code')
        lu_region = make_lookup(ipm, 'region_code')
        lu_continent = make_lookup(ipm, 'continent_code')
        lu_city = make_lookup(ipm, 'city')

        df[country] = df[field].apply(lambda ip: lu_country(ip))
        df[region] = df[field].apply(lambda ip: lu_region(ip))
        df[continent] = df[field].apply(lambda ip: lu_continent(ip))
        df[city] = df[field].apply(lambda ip: lu_city(ip))

        total = len(df.index)
        print(f'total          = {total:>7}')

        def list_missing(key):
            missing = df[key].isnull().sum()
            print(
                f'missing {key:20} = {missing:>7} ({round(missing / total * 100, 2)}%)')

        list_missing(country)
        list_missing(region)
        list_missing(continent)
        list_missing(city)

    fn = fn.replace('csv.gz', 'geo.csv.gz')
    print(f'writing "{fn}"')
    df.to_csv(fn, index=False, sep="|")


if __name__ == '__main__':
    main()
