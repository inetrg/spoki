"""
Calculate the top ports from assembled events.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import click
import csv
import glob
import gzip
import ipaddress
import json
import os

import matplotlib.pyplot as plt
import pandas as pd

from collections import defaultdict
from pathlib import Path
from tqdm import tqdm


# -- defaults ------------------------------------------------------------------


my_cs = ["#375E97", "#FB6542", "#4ce888"]
markers = [".", "1", "x", "+", "2", "3", "4", "^", "v", "s", "D"]
plt.rcParams.update({"font.size": 12})
plt.rc("figure", figsize=(8 * 0.7, 3 * 0.7))

DEFAULT_SCAN_SOURCE_PATH = "./known-scanners"

# -- scanner identification ----------------------------------------------------


class KnownScanners:
    """
    Filter IP source address to check if they are in a prefix known to research
    scan projects, etc.
    """

    # known_prefixes: dict[str, list[ipaddress.IPv4Network]] = {}
    # ip_lists: list[tuple[str, list[ipaddress.IPv4Network]] = []

    # -- Some known prefixes -----------------------------------------

    # Censys: 198.108.66.0/23 and 192.35.168.0/23
    censys = [
        ipaddress.IPv4Network("198.108.66.0/23"),
        ipaddress.IPv4Network("192.35.168.0/23"),
    ]
    # Don't know
    kudelski = [ipaddress.IPv4Network("185.35.62.0/23")]
    # https://opendata.rapid7.com/about/
    rapid7 = [
        ipaddress.IPv4Network("5.63.151.96/27"),
        ipaddress.IPv4Network("71.6.233.0/24"),
        ipaddress.IPv4Network("88.202.190.128/27"),
        ipaddress.IPv4Network("146.185.25.160/27"),
        ipaddress.IPv4Network("109.123.117.224/27"),
    ]
    # In use by rapid7 prior to August 23, 2018
    # [ipaddress.IPv4Network('71.6.216.32/27'),
    #       ipaddress.IPv4Network('216.98.153.224/27')]
    # Asked them via the contact form on the website.
    shadowserver = [
        ipaddress.IPv4Network("184.105.139.64/26"),
        ipaddress.IPv4Network("184.105.247.192/26"),
        ipaddress.IPv4Network("216.218.206.64/26"),
        ipaddress.IPv4Network("74.82.47.0/26"),
        ipaddress.IPv4Network("65.49.20.64/26"),
    ]

    # -- initializer -------------------------------------------------

    def __init__(self):
        self.known_prefixes = {
            "censys": self.censys,
            "kudelski": self.kudelski,
            "rapid7": self.rapid7,
            "shadowserver": self.shadowserver,
        }
        self.ip_lists = []

    # -- members -----------------------------------------------------

    def load_scanners(self, scanner_lists_root):
        if not Path(scanner_lists_root).is_dir():
            print(f"WARN: not a folder: '{scanner_lists_root}'")
            return
        pattern = os.path.join(scanner_lists_root, "ips.*.txt")
        filenames = glob.glob(pattern)

        def get_name(fn):
            fn = os.path.basename(fn)
            fn = fn.replace("ips.", "")
            fn = fn.replace(".txt", "")
            return fn

        names = [get_name(fn) for fn in filenames]
        print(f"loading {len(names)} additional scan lists from '{scanner_lists_root}'")
        for name in names:
            print(f"loading 'ips.{name}.txt'")
            absolute = os.path.join(scanner_lists_root, f"ips.{name}.txt")
            fn = Path(absolute)
            if not fn.is_file():
                print(f'ERR: "{fn}" does not exist')
                continue
            ips = set()
            with open(fn, "r") as fh:
                for line in fh:
                    ip = line.strip("\n")
                    ips.add(ipaddress.IPv4Address(ip))
            if len(ips) == 0:
                print(f"ERR: found no entries in {fn}")
                return
            # print(f"loaded {len(ips)} addresses for {name}")
            # print(ips)
            self.ip_lists.append((name, ips))

    def is_in(self, ipv4, prefixes):
        for prefix in prefixes:
            if ipv4 in prefix:
                return True
        return False

    def check_prefixes(self, ipv4):
        for tag, prefixes in self.known_prefixes.items():
            if self.is_in(ipv4, prefixes):
                return tag
        return "unknown"

    def check_ip_lists(self, ipv4):
        # print(f"got {len(self.ip_lists)} ip lists to check")
        for tag, ip_list in self.ip_lists:
            # print(f"> {tag} has {len(ip_list)} entries")
            if ipv4 in ip_list:
                return tag
        # exit()
        return "unknown"

    def check(self, source):
        ipv4 = ipaddress.IPv4Address(source)
        tag = self.check_prefixes(ipv4)
        if tag == "unknown":
            tag = self.check_ip_lists(ipv4)
        return tag


# -- files ---------------------------------------------------------------------


def file_exists(fn):
    return Path(fn).is_file()


# -- main ----------------------------------------------------------------------


@click.command()
@click.argument(
    "folder",
    type=click.Path(exists=True),
)
@click.option(
    "--vantagepoint",
    "-v",
    "datasource",
    required=True,
    help="vantage point where the logs were captured",
)
@click.option(
    "--scanner-list-dir",
    "-s",
    "lists",
    default=DEFAULT_SCAN_SOURCE_PATH,
    type=click.Path(exists=True),
    help="set folder with additional scanner info",
)
@click.option(
    "--days",
    "-d",
    default=91,
    type=int,
    help="number of days to analyze",
)
@click.option(
    "--force",
    "-f",
    "force_recalculation",
    is_flag=True,
    help="force recalculation",
)
def main(folder, datasource, days, force_recalculation, lists):
    # setup scanner identification.
    ks = KnownScanners()
    ks.load_scanners(lists)

    recalculated_ports = False

    ev_types = ["isyn", "rsyn", "two-phase"]

    csv_files = [f"{datasource}.ports.{tag}.{days}.csv" for tag in ev_types]
    found_csv_files = all([file_exists(fn) for fn in csv_files])

    # Read data into these
    two_phase_ports = defaultdict(lambda: 0)
    regular_sequence_ports = defaultdict(lambda: 0)
    irregular_sequence_ports = defaultdict(lambda: 0)

    two_phase_ks = 0
    regular_sequence_ks = 0
    irregular_sequence_ks = 0

    if not found_csv_files or force_recalculation:
        print("counting port stats from assembled files")
        recalculated_ports = True

        if not Path(folder).is_dir():
            print(f"ERR: not a folder: {folder}")
            return

        pattern = f"{datasource}-events-*.json.gz"
        pattern_with_path = os.path.join(folder, pattern)
        files = glob.glob(pattern_with_path)

        if len(files) == 0:
            print(f"ERR: no matching files in {folder}")
            return

        files.sort()
        files_to_process = days * 24
        files = files[:files_to_process]

        num_files = len(files)
        for file in tqdm(files, total=num_files):
            # print(f"[{i + 1}/{num_files}] reading {file}")
            with gzip.open(file, "rt") as fh:
                for line in fh:
                    ev = json.loads(line)
                    tag = ev["tag"]
                    if tag in ["isyn (acked)", "isyn"]:
                        pkt = ev["isyn"]
                        dport = int(pkt["trigger"]["payload"]["dport"])
                        saddr = pkt["trigger"]["saddr"]
                        project = ks.check(saddr)
                        if project == "unknown":
                            irregular_sequence_ports[dport] += 1
                        else:
                            irregular_sequence_ks += 1
                    elif tag in ["rsyn (acked)", "rsyn"]:
                        pkt = ev["rsyn"]
                        dport = int(pkt["trigger"]["payload"]["dport"])
                        saddr = pkt["trigger"]["saddr"]
                        project = ks.check(saddr)
                        if project == "unknown":
                            regular_sequence_ports[dport] += 1
                        else:
                            regular_sequence_ks += 1
                    elif tag in ["two-phase (no ack)", "two-phase"]:
                        pkt = ev["isyn"]
                        dport = int(pkt["trigger"]["payload"]["dport"])
                        saddr = pkt["trigger"]["saddr"]
                        project = ks.check(saddr)
                        if project == "unknown":
                            two_phase_ports[dport] += 1
                        else:
                            two_phase_ks += 1
                    elif tag in ["ack"]:
                        pass
                    else:
                        print(f"ERR: unexpected tag: {tag}")

        # -- write data to disk ---------------------------------------------------

        fn = f"{datasource}-top-ports.{days}.json"
        print(f"writing {fn}")
        with open(fn, "w") as fh:
            d = {
                "rs": regular_sequence_ports,
                "is": irregular_sequence_ports,
                "tp": two_phase_ports,
                "ks": {
                    "rs": regular_sequence_ks,
                    "is": irregular_sequence_ks,
                    "tp": two_phase_ks,
                },
            }
            json.dump(d, fh)

    else:
        print("reading existing CSV files")

        def read_csv(tag):
            fn = f"{datasource}.ports.{tag}.{days}.csv"
            counts = defaultdict(lambda: 0)
            with open(fn, "rt") as fh:
                reader = csv.reader(fh, delimiter="|")
                header = next(reader)
                print(f"skipping header: '{header}'")
                for row in reader:
                    port = int(row[0])
                    count = int(row[1])
                    counts[port] = count
            return counts

        two_phase_ports = read_csv("two-phase")
        regular_sequence_ports = read_csv("rsyn")
        irregular_sequence_ports = read_csv("isyn")

        # for port, count in two_phase_ports.items():
        # irregular_sequence_ports[port] += count

    # -- print top ports ------------------------------------------------------

    def top(data, n=25):
        tups = list(data.items())
        total = sum([y for x, y in tups])
        tups.sort(key=lambda x: x[1], reverse=True)
        print(f"total:                     {total:>13,}")
        print(f"top {n}")
        print(f"{'port':>12}, {'count':>8}, {'share':>6}")
        for k, v in tups[:n]:
            pct = round(v * 100 / total, 2)
            print(f"{k:>12}, {v:>8}, {pct:>5.2f}%")

    def additional_data(data, n):
        tups = list(data.items())
        num_port = len(tups)
        print(f"number of ports tageted:          {num_port:>5}")

    additional_n = 25

    print("# regular sequences")
    if regular_sequence_ks != 0:
        print(f"excluded scanners: {regular_sequence_ks}")
    top(regular_sequence_ports)
    additional_data(regular_sequence_ports, additional_n)
    print("")

    print("# irrgular sequences")
    if irregular_sequence_ks != 0:
        print(f"excluded scanners: {irregular_sequence_ks}")
    top(irregular_sequence_ports)
    additional_data(irregular_sequence_ports, additional_n)
    print("")

    print("# two phase")
    if two_phase_ks != 0:
        print(f"excluded scanners: {two_phase_ks}")
    top(two_phase_ports)
    additional_data(two_phase_ports, additional_n)

    def share_line(data, n):
        tups = list(data.items())
        total = sum([y for x, y in tups])
        tups.sort(key=lambda x: x[1], reverse=True)
        tups = tups[:n]
        shares = [round(v * 100 / total, 2) for _, v in tups]
        return shares

    rshares = share_line(regular_sequence_ports, 25)
    ishares = share_line(irregular_sequence_ports, 25)
    tpshares = share_line(two_phase_ports, 25)

    df = pd.DataFrame()
    df["isyn"] = ishares
    df["rsyn"] = rshares
    df["two-phase"] = tpshares

    ax = df.plot(color=my_cs, legend=False, figsize=(8 * 0.7, 3 * 0.7), alpha=0.8)
    ax.set_xlabel("Top Ports by Rank [#]")
    ax.set_ylabel("Share [%]")
    ax.legend(
        loc="upper center",
        bbox_to_anchor=(0.49, 1.3),
        ncol=2,
        fancybox=False,
        shadow=False,
    )
    # ax.set_ylim(ylim)
    fig = ax.get_figure()
    fn = f"{datasource}.top-ports-comparison.pdf"
    print(f"plotting {fn}")
    fig.savefig(fn, bbox_inches="tight")
    plt.close(fig)

    # -- make bar ports -------------------------------------------------------

    def plot_bars(data, tag, n=25):
        total = sum(list(data.values()))
        data = [(x, round(y * 100 / total, 2)) for x, y, in data.items()]
        data.sort(key=lambda x: x[1], reverse=True)
        data = data[:n]
        df = pd.DataFrame(data, columns=["port", "share"])
        ax = df.plot.bar(x="port", y="share", rot=0, legend=False)

        ax.set_xlabel("Port [#]")
        ax.set_ylabel("Share [%]")

        # ax.set_xticklabels(rotation=90)
        ax.tick_params(axis="x", labelrotation=90)

        ax.tick_params(bottom=True, top=False, left=True, right=True)
        ax.tick_params(bottom=True, top=False, left=True, right=True)
        ax.tick_params(
            labelbottom=True, labeltop=False, labelleft=True, labelright=False
        )

        ax.tick_params(which="major", axis="y", direction="in")

        # save
        fig = ax.get_figure()
        # fig.tight_layout(pad=0.5)
        fn = f"{datasource}-top-ports-{tag}.pdf"
        print(f"plotting {fn}")
        fig.savefig(fn, bbox_inches="tight")
        plt.close(fig)

    plot_bars(regular_sequence_ports, "rs")
    plot_bars(irregular_sequence_ports, "is")
    plot_bars(two_phase_ports, "tp")

    # -- dump -----------------------------------------------------------------

    if recalculated_ports:

        def dump(data, tag):
            tups = list(data.items())
            total = sum([y for x, y in tups])
            tups.sort(key=lambda x: x[0])
            fn = f"{datasource}.ports.{tag}.{days}.csv"
            print(f"dumping '{tag}' to '{fn}'")
            with open(fn, "w") as fh:
                writer = csv.writer(fh, delimiter="|")
                writer.writerow(["port", "count", "share"])
                for port, num in tups:
                    pct = round(num * 100 / total, 2)
                    writer.writerow([port, num, pct])

        dump(regular_sequence_ports, "rsyn")
        dump(irregular_sequence_ports, "isyn")
        dump(two_phase_ports, "two-phase")


if __name__ == "__main__":
    main()
