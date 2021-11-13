#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
assemble:
    "Fit together the separate component parts of (a machine or other object)."

This script reads in Spoki logs files to assembel two-phase events.

Example:
  assemble -s 2020-10-03 -H 0 -d bcix-nt -e 2020-10-04 -P 600 -c ./data/
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

# Standard imports.
from datetime import datetime, timedelta, timezone
from pathlib import Path

# External modules.
import click

# This module.
from cse.logs.confirmationmatcher import ConfirmationMatcher
from cse.logs.eventmatcher import EventMatcher
from cse.logs.phasematcher import PhaseMatcher
from cse.logs.reader import LiveReader, SpokiFileFactory, SwiftFileFactory
from cse.logs.writer import KafkaWriter, LogWriter
from cse.types.event import Event
from cse.types.probe_confirmation import ProbeConfirmation


@click.command()
@click.argument(
    "log_dir",
    type=click.Path(exists=True),
    # help="read log files form this directory",
)
# -- options -----------
@click.option(
    "-s",
    "--start-date",
    "start_date",
    default=None,
    required=True,
    help="process data starting at this day (YYYY-MM-DD)",
    type=str,
)
@click.option(
    "-H",
    "--hour",
    "hour",
    default=0,
    type=int,
    help="skip the first N hours of the first day (default: 0)",
)
@click.option(
    "-d",
    "--datasource",
    "datasource",
    type=str,
    default=None,
    required=True,
    help="match phases for this datasource",
)
@click.option(
    "-t",
    "--probe-timeout",
    "probe_timeout",
    default=5,
    type=int,
    help="max interval between a req. and its conf. in mins (default: 5)",
)
@click.option(
    "-P",
    "--phase-timeout",
    "phase_timeout",
    default=600,
    type=int,
    help="max interval between to phases in seconds (default: 600)",
)
@click.option(
    "-e",
    "--end-date",
    "end_date",
    type=str,
    help="process data until this day (YYYY-MM-DD)",
)
@click.option(
    "-k",
    "--kafka-port",
    "kafka_port",
    type=int,
    default=9092,
    help="port of the local kafka server (default: 9092)",
)
@click.option(
    "--kafka-batch-size",
    "kafka_batch_size",
    type=int,
    default=1000,
    help="batch size for sending produced events (default: 1000)",
)
@click.option(
    "-o",
    "--output",
    "out_dir",
    type=click.Path(exists=True),
    help="choose the output dir for non-kafka writer (default: .)",
    default=".",
)
# -- flags -------------
@click.option(
    "--swift/--local",
    default=False,
    help="toggle between reading data from local disk and from OpenStack Swift (default: local)",
)
@click.option(
    "--kafka/--logs",
    default=False,
    help="toggle writing results to Kafka and to logs on disk (default: logs)",
)
@click.option(
    "--compressed",
    is_flag=True,
    help="read gzip compressed files from disk",
)
@click.option(
    "--csv/--json",
    "read_csv",
    default=True,
    help="toggle between CSV and JSON input files (default: CSV)",
)
def main(
    log_dir,
    start_date,
    hour,
    datasource,
    probe_timeout,
    phase_timeout,
    end_date,
    kafka_port,
    kafka_batch_size,
    out_dir,
    swift,
    kafka,
    compressed,
    read_csv,
):
    start_date = start_date
    hour = hour
    datasource = datasource

    sy, sm, sd = tuple(start_date.split("-"))
    start_date = datetime(
        int(sy),
        int(sm),
        int(sd),
        tzinfo=timezone.utc,
    )
    start_hour = datetime(
        int(sy),
        int(sm),
        int(sd),
        int(hour),
        tzinfo=timezone.utc,
    )

    if end_date is None:
        end_date = start_date + timedelta(days=1)
    else:
        ey, em, ed = tuple(end_date.split("-"))
        end_date = datetime(int(ey), int(em), int(ed), tzinfo=timezone.utc)

    log_dir = log_dir
    if not Path(log_dir).is_dir():
        print("please set a directory with -l")
        return
    print(f'reading data from disk: "{log_dir}"')

    scamper_tag = "scamper" if read_csv else "scamper-responses"

    # File factories.
    if swift:
        event_file_factory = SwiftFileFactory(
            Event,
            datasource,
            "tcp",
            "raw",
            read_csv,
        )
        con_file_factory = SwiftFileFactory(
            ProbeConfirmation,
            datasource,
            "tcp",
            scamper_tag,
            read_csv,
        )
    else:
        event_file_factory = SpokiFileFactory(
            Event,
            log_dir,
            datasource,
            "tcp",
            "raw",
            compressed,
            read_csv,
        )
        con_file_factory = SpokiFileFactory(
            ProbeConfirmation,
            log_dir,
            datasource,
            "tcp",
            scamper_tag,
            compressed,
            read_csv,
        )

    # File reader.
    event_reader = LiveReader(
        event_file_factory,
        start_hour,
    )
    con_reader = LiveReader(
        con_file_factory,
        start_hour,
    )

    probe_timeout = timedelta(minutes=probe_timeout)
    phase_timeout = timedelta(seconds=phase_timeout)

    s = start_date.strftime("%Y-%m-%d")
    e = end_date.strftime("%Y-%m-%d")
    print(f"looking for sequences starting between {s} @{hour}h and {e}")

    kafka_producer_topic = f"cse2.malware.events.{datasource}"

    print(f"will publish to '{kafka_producer_topic}'")

    assert (
        event_reader.next_hour == con_reader.next_hour
    ), "Readers should start at the same hour"
    assert event_reader.next_hour <= end_date, "End must be after start"

    cm = ConfirmationMatcher(con_reader, event_reader, probe_timeout)
    pm = PhaseMatcher(phase_timeout)

    wr = None
    if kafka:
        wr = KafkaWriter(
            kafka_producer_topic,
            [f"localhost:{kafka_port}"],
            kafka_batch_size,
        )
    else:
        if not Path(out_dir).is_dir():
            print("please select a directory with -o/--output")
            return
        wr = LogWriter(datasource, out_dir)

    em = EventMatcher(cm, pm, start_hour, wr)
    em.match()
