#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
These classes reads JSON data from a variety sources and parse them into
objects that support a `from_dict` method.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import csv
import gzip
import json
import os
import sys
import wandio

from datetime import datetime, timedelta, timezone
from pathlib import Path
from time import sleep, perf_counter

# Build names for swift files.
swift_container = "stardust-spoki"
swift_fn = "datasource={}/protocol={}/type={}/{}/{}.spoki.{}.{}.{}.{}.gz"

# Example: 2020-09-07.07:00:00.ucsd-nt-reactive.spoki.icmp.raw.1599487200.json
uncompressed_pattern = "{}.{}.spoki.{}.{}.{}.{}"
compressed_pattern = uncompressed_pattern + ".gz"

# Bail after missing files for two days
CONTINUOUSLY_MISSING_LIMIT = 24 * 2

SKIP_TIME = timedelta(hours=1, minutes=5)


class Reader:
    def __init__(self, EntryType, read_csv=False):
        self.type = EntryType
        self.read_csv = read_csv

    # -- read data from disk --------------------------------------------------
    def from_disk(self, path, start_hour, datasource, proto, log_type):
        """
        Read compressed log from disk.
        """
        hour = start_hour
        while True:
            unix_ts = int(datetime.timestamp(hour))
            current_hour = hour.strftime("%Y-%m-%d.%H:%M:%S")
            ending = "csv" if self.read_csv else "json"
            filename = compressed_pattern.format(
                current_hour, datasource, proto, log_type, unix_ts, ending
            )
            print(f'{current_hour}: "{filename}"')
            fh = None
            try:
                # load from disk ...
                absolute = os.path.join(path, filename)
                fh = gzip.open(absolute, "r")
            except FileNotFoundError:
                print(f"[WARN] file not found: {filename}")
                return
            except Exception:
                print(f"[WARN] opening {filename}: ({sys.exc_info()[0]})")
                return
            # ... and iterate lines
            for line in fh:
                obj = json.loads(line)
                yield self.type.from_dict(obj)
            hour += timedelta(hours=1)

    # -- read data from swift -------------------------------------------------


class HourlyReader:
    def __init__(
        self,
        EntryType,
        path,
        start_hour,
        datasource,
        proto,
        log_type,
        read_csv,
    ):
        self.type = EntryType
        self.path = path
        self.next_hour = start_hour
        self.datasource = datasource
        self.proto = proto
        self.log_type = log_type
        self.read_csv = read_csv

    def next(self, batch_id=None):
        unix_ts = int(datetime.timestamp(self.next_hour))
        date_str = self.next_hour.strftime("%Y-%m-%d.%H:%M:%S")
        ending = "csv" if self.read_csv else "json"
        filename = compressed_pattern.format(
            date_str,
            self.datasource,
            self.proto,
            self.log_type,
            unix_ts,
            ending,
        )
        print(f'in: "{filename}"')
        fh = None
        try:
            absolute = os.path.join(self.path, filename)
            fh = gzip.open(absolute, "r")
        except FileNotFoundError:
            print(f"[WARN] file not found: {filename}")
            return []
        except Exception:
            print(f"[WARN] opening {filename}: ({sys.exc_info()[0]})")
            return []
        # ... and iterate lines
        items = []
        if batch_id is None:
            for line in fh:
                obj = json.loads(line)
                items.append(self.type.from_dict(obj))
        else:
            for line in fh:
                obj = json.loads(line)
                elem = self.type.from_dict(obj)
                elem.set_batch_id(batch_id)
                items.append(elem)
        self.next_hour += timedelta(hours=1)
        return items

    def up_next(self):
        return self.next_hour


class HourlySwiftReader:
    def __init__(
        self,
        EntryType,
        start_hour,
        datasource,
        proto,
        log_type,
        read_csv,
    ):
        self.type = EntryType
        self.next_hour = start_hour
        self.datasource = datasource
        self.proto = proto
        self.log_type = log_type
        self.read_csv = read_csv

    def next(self, batch_id=None):
        """
        Read data from OpenStack via wandio. Requires access to CAIDA data
        store.
        """
        unix_ts = int(datetime.timestamp(self.next_hour))
        # file_date_components = hour.strftime("%Y-%m-%d")
        file_date = self.next_hour.strftime("year=%Y/month=%m/day=%d")
        # y, m, d = file_date_components.split('-')
        ending = "csv" if self.read_csv else "json"
        filename = swift_fn.format(
            self.datasource,
            self.proto,
            self.log_type,
            file_date,
            self.datasource,
            self.proto,
            self.log_type,
            unix_ts,
            ending,
        )
        print(f'in: "{filename}"')
        fh = None
        try:
            fh = wandio.open(f"swift://{swift_container}/{filename}", "r")
        except FileNotFoundError:
            print(f"[WARN] file not found: {filename}")
            return []
        except Exception:
            print(f"[WARN] opening {filename}: ({sys.exc_info()[0]})")
            return []
        # ... and iterate lines
        items = []
        if batch_id is None:
            for line in fh:
                obj = json.loads(line)
                items.append(self.type.from_dict(obj))
        else:
            for line in fh:
                obj = json.loads(line)
                elem = self.type.from_dict(obj)
                elem.set_batch_id(batch_id)
                items.append(elem)
        self.next_hour += timedelta(hours=1)
        return items

    def up_next(self):
        return self.next_hour


#
# We have a LiveReader that takes a FileFactory that it uses to create new log
# files. These files provide the following interface:
#  `consume(self, n=DEFAULT)`: Read up to n events from the file.
#  `expect_more(self)`: Return True if there is likely more data in the file.
#  `exists(self)`: Check if the file was created yet.
#  `is_open(self)`: Check if the file was already opened.
#  `open(self)`: Open the file, requires that it exists.
# And the file must have a member batch_id assigned when creating the file
#

# -- spoki log file ----------------------------------------------------------


class SpokiFileFactory:
    def __init__(
        self,
        EntryType,
        path,
        datasource,
        proto,
        log_type,
        compressed,
        read_csv,
    ):
        self.type = EntryType
        self.path = path
        self.datasource = datasource
        self.proto = proto
        self.log_type = log_type
        self.compressed = compressed
        self.read_csv = read_csv

    def make(self, hour, batch_id):
        return SpokiLogFile(
            self.type,
            self.path,
            hour,
            self.datasource,
            self.proto,
            self.log_type,
            batch_id,
            self.compressed,
            self.read_csv,
        )


class JSONFile:
    def __init__(self, EntryType, filepath, batch_id, compressed):
        self.type = EntryType
        self.filepath = filepath
        self.batch_id = batch_id
        self.compressed = compressed

        self.path = Path(self.filepath)
        self.fh = None

        self.last_line = None
        self.last_read_got_data = True
        self.lines_read = 0

    def exists(self):
        return self.path.is_file()

    def is_open(self):
        return self.fh is not None

    def open(self):
        if not self.exists():
            return False
        try:
            if self.compressed:
                self.fh = gzip.open(self.filepath, "r")
            else:
                self.fh = open(self.filepath, "r")
            print(f'opened "{self.filepath}"')
            return True
        except FileNotFoundError:
            print(f"[WARN] file not found: {self.filepath}")
            return False
        except Exception:
            print(f"[WARN] opening {self.filepath}: ({sys.exc_info()[0]})")
            return False

    def consume(self, num=100000):
        # Try to read num entries (or as much as possible if `num` is `None`)
        # keep last read time and number of consumed lines for stats.

        if self.fh is None:
            print("[WARN] consume called with no open file")
            return []

        eof = b"" if self.compressed else ""

        cnt = 0
        line_cache = []
        while True:
            cnt += 1
            line = self.fh.readline()
            if line is None or line == eof:
                break
            line_cache.append(line)
            if cnt >= num:
                break

        items = []

        def handle_json_error(line):
            if line is None:
                print("[WARN] line is None, skipping it")
                return
            if line == "":
                print("[WARN] line is empty")
                return
            if self.last_line is None:
                self.last_line = line
            else:
                self.last_line += line
                if not self.last_line.endswith("\n"):
                    return
                try:
                    print("[WARN] found complete line")
                    obj = json.loads(self.last_line)
                    elem = self.type.from_dict(obj)
                    if self.batch_id is not None:
                        elem.set_batch_id(self.batch_id)
                    items.append(elem)
                    self.lines_read += 1
                    print("[WARN] found JSON object!")
                    self.last_line = None
                except json.decoder.JSONDecodeError:
                    print(f'[ERR] dropping: "{self.last_line}"')
                    self.last_line = None
                except TypeError:
                    print(f'[ERR] type error in line: "{self.last_line}"')
                    self.last_line = None
                except KeyError:
                    print(f'[ERR] key error in line: "{self.last_line}"')
                    self.last_line = None

        if self.batch_id is None:
            for line in line_cache:
                try:
                    obj = json.loads(line)
                    if obj is None:
                        handle_json_error(line)
                    else:
                        items.append(self.type.from_dict(obj))
                        self.lines_read += 1
                except json.decoder.JSONDecodeError:
                    print(f'[WARN] skipping line "{line}"')
                    handle_json_error(line)
                except TypeError:
                    print(f'[WARN] type error in line: "{line}"')
                except KeyError:
                    print(f'[WARN] key error in line: "{line}"')
        else:
            for line in line_cache:
                try:
                    obj = json.loads(line)
                    if obj is None:
                        handle_json_error(line)
                    else:
                        elem = self.type.from_dict(obj)
                        elem.set_batch_id(self.batch_id)
                        items.append(elem)
                        self.lines_read += 1
                except json.decoder.JSONDecodeError:
                    print(f'[WARN] skipping line "{line}"')
                    handle_json_error(line)
                except TypeError:
                    print(f'[WARN] type error in line: "{line}"')
                except KeyError:
                    print(f'[WARN] key error in line: "{line}"')

        if len(items) > 0:
            self.last_read_got_data = True
        else:
            self.last_read_got_data = False
        return items

    def expect_more(self):
        if self.last_read_got_data:
            return True
        else:
            return False


class CSVFile:
    def __init__(self, EntryType, filepath, batch_id, compressed):
        self.type = EntryType
        self.filepath = filepath
        self.batch_id = batch_id
        self.compressed = compressed

        self.path = Path(self.filepath)
        self.fh = None
        self.reader = None

        self.last_read_got_data = True
        self.lines_read = 0

        self.header = []
        self.buffer = ""
        self.found_header = False

    def exists(self):
        return self.path.is_file()

    def is_open(self):
        return self.fh is not None

    def open(self):
        if not self.exists():
            return False
        try:
            if self.compressed:
                self.fh = gzip.open(self.filepath, "rt")
            else:
                self.fh = open(self.filepath, "rt")
            print(f'opened "{self.filepath}"')
        except FileNotFoundError:
            print(f"[WARN] file not found: {self.filepath}")
            return False
        except Exception:
            print(f"[WARN] opening {self.filepath}: ({sys.exc_info()[0]})")
            return False
        return True

    def consume(self, num=100000):
        # Try to read num entries (or as much as possible if `num` is `None`)
        # keep last read time and number of consumed lines for stats.

        if self.fh is None:
            print("[WARN] consume called with no open file")
            return []

        eof = b"" if self.compressed else ""

        cnt = 0
        line_cache = []
        while True:
            line = self.fh.readline()
            if line == "":
                break
            self.buffer += line
            if self.buffer.endswith('\n'):
                if self.found_header:
                    sanitized = self.buffer.rstrip()
                    parts = sanitized.split('|')
                    # print(f"[<] {parts}")
                    obj = {k: v for k, v in zip(self.header, parts)}
                    line_cache.append(obj)
                    self.buffer = ""
                    cnt += 1
                else:
                    sanitized = self.buffer.rstrip()
                    self.header = sanitized.split('|')
                    self.found_header = True
                    # print(f"[h] '{self.header}'")
                    self.buffer = ""
            else:
                break

        items = []

        if self.batch_id is None:
            for line in line_cache:
                try:
                    items.append(self.type.from_csv(line))
                    self.lines_read += 1
                except TypeError as te:
                    print(f'[WARN] type error in line: "{line}": {te}')
                except KeyError as ke:
                    print(f'[WARN] key error in line: "{line}": {ke}')
        else:
            for line in line_cache:
                try:
                    elem = self.type.from_csv(line)
                    elem.set_batch_id(self.batch_id)
                    items.append(elem)
                    self.lines_read += 1
                except TypeError as te:
                    print(f'[WARN] type error in line: "{line}": {te}')
                except KeyError as ke:
                    print(
                        f'[WARN] key error in line: "{line}": {ke} in type {self.type}'
                    )

        if len(items) > 0:
            self.last_read_got_data = True
        else:
            self.last_read_got_data = False
        return items

    def expect_more(self):
        if self.last_read_got_data:
            return True
        else:
            return False


class SpokiLogFile:
    """
    An open log file that might still be written at the moment.
    """

    def __init__(
        self,
        EntryType,
        path,
        hour,
        datasource,
        proto,
        log_type,
        batch_id,
        compressed=True,
        read_csv=False,
    ):
        self.type = EntryType
        self.path = path
        self.hour = hour
        self.datasource = datasource
        self.proto = proto
        self.log_type = log_type
        self.batch_id = batch_id
        self.compressed = compressed
        self.lines_read = 0
        self.read_csv = read_csv

        self.timeout = hour + timedelta(hours=1, minutes=10)

        pattern = compressed_pattern if compressed else uncompressed_pattern
        unix_ts = int(datetime.timestamp(self.hour))
        date_str = self.hour.strftime("%Y-%m-%d.%H:%M:%S")
        ending = "csv" if self.read_csv else "json"
        self.filename = pattern.format(
            date_str,
            self.datasource,
            self.proto,
            self.log_type,
            unix_ts,
            ending,
        )
        self.filepath = Path(os.path.join(path, self.filename))
        print(f'upcoming log file: "{self.filepath}"')

        if read_csv:
            self.openfile = CSVFile(
                EntryType, self.filepath, self.batch_id, self.compressed
            )
        else:
            self.openfile = JSONFile(
                EntryType, self.filepath, self.batch_id, self.compressed
            )

        self.last_read = datetime.now(tz=timezone.utc)
        self.last_read_got_data = True
        self.stats = []

    def exists(self):
        return self.openfile.exists()

    def is_open(self):
        return self.openfile.is_open()

    def open(self):
        return self.openfile.open()

    def consume(self, num=100000):
        consume_start = perf_counter()
        items = self.openfile.consume(num)

        if len(items) > 0:
            self.last_read_got_data = True
            self.last_read = datetime.now(tz=timezone.utc)
            duration = perf_counter() - consume_start
            self.stats.append((self.last_read, len(items), duration))
        else:
            self.last_read_got_data = False

        return items

    def expect_more(self):
        if self.openfile.expect_more():
            return True
        # Check if current time is after start_hour + 1 hour.
        current_time = datetime.now(tz=timezone.utc)
        if current_time > self.timeout:
            return False
        else:
            # Maybe: return false if we couldn't read anything in the last
            #  10 minutes?
            # Need a different mechanism for offline files.
            return True


# -- swift log file ----------------------------------------------------------


class SwiftFileFactory:
    def __init__(self, EntryType, datasource, proto, log_type, read_csv):
        self.type = EntryType
        self.datasource = datasource
        self.proto = proto
        self.log_type = log_type
        self.read_csv = read_csv

    def make(self, hour, batch_id):
        return SwiftLogFile(
            self.type,
            hour,
            self.datasource,
            self.proto,
            self.log_type,
            self.read_csv,
            batch_id,
        )


class SwiftLogFile:
    """
    An open log file that might still be written at the moment.
    """

    def __init__(
        self, EntryType, hour, datasource, proto, log_type, read_csv, batch_id
    ):
        self.type = EntryType
        self.hour = hour
        self.datasource = datasource
        self.proto = proto
        self.log_type = log_type
        self.batch_id = batch_id
        self.lines_read = 0
        self.read_csv = read_csv

        # create filename
        unix_ts = int(datetime.timestamp(self.hour))
        file_date = self.hour.strftime("year=%Y/month=%m/day=%d")
        ending = "csv" if self.read_csv else "json"
        self.filename = swift_fn.format(
            self.datasource,
            self.proto,
            self.log_type,
            file_date,
            self.datasource,
            self.proto,
            self.log_type,
            unix_ts,
            ending,
        )
        print(f'upcoming log file: "{self.filename}"')

        self.fh = None
        self.last_read = datetime.now(tz=timezone.utc)
        self.read_everything = False
        self.stats = []

    def exists(self):
        try:
            with wandio.open(f"swift://{swift_container}/{self.filename}"):
                return True
        except FileNotFoundError:
            return False
        except Exception:
            return False

    def is_open(self):
        return self.fh is not None

    def open(self):
        try:
            self.fh = wandio.open(f"swift://{swift_container}/{self.filename}")
            return True
        except FileNotFoundError:
            print(f"[WARN] file not found: {self.filename}")
            return False
        except Exception:
            print(f"[WARN] opening {self.filename}: ({sys.exc_info()[0]})")
            return False

    def consume(self, num=100000):
        # Try to read num entries (or as much as possible if `num` is `None`)
        # keep last read time and number of consumed lines for stats.
        consume_start = perf_counter()

        cnt = 0
        line_cache = []
        if num is None:
            for line in self.fh:
                line_cache.append(line)
            self.read_everything = True
        else:
            while cnt < num:
                line = self.fh.readline()
                if line is None or line == b"":
                    self.read_everything = True
                    break
                line_cache.append(line)
                cnt += 1

        items = []
        if self.batch_id is None:
            for line in line_cache:
                try:
                    obj = json.loads(line)
                    if obj is None:
                        print(f'[ERR] dropping: "{line}"')
                    else:
                        items.append(self.type.from_dict(obj))
                        self.lines_read += 1
                except json.decoder.JSONDecodeError:
                    print(f'[WARN] skipping line "{line}"')
                except TypeError:
                    print(f'[WARN] type error in line: "{line}"')
        else:
            for line in line_cache:
                try:
                    obj = json.loads(line)
                    if obj is None:
                        print(f'[ERR] dropping: "{line}"')
                    else:
                        elem = self.type.from_dict(obj)
                        elem.set_batch_id(self.batch_id)
                        items.append(elem)
                        self.lines_read += 1
                except json.decoder.JSONDecodeError:
                    print(f'[WARN] skipping line "{line}"')
                except TypeError:
                    print(f'[WARN] type error in line: "{line}"')

        if len(items) > 0:
            self.last_read = datetime.now(tz=timezone.utc)
            duration = perf_counter() - consume_start
            self.stats.append((self.last_read, cnt, duration))

        return items

    def expect_more(self):
        if self.read_everything:
            return False
        return True


# -- live log reader ---------------------------------------------------------


class LiveReader:
    def __init__(
        self, log_file_factory, start_hour, initial_batch_id=0, sleep_seconds=5
    ):
        self.next_hour = start_hour
        self.batch_id = initial_batch_id
        self.sleep_seconds = sleep_seconds
        self.log_file_factory = log_file_factory
        self.upcoming_file = None
        self.files = {}

        self.prepare_next_file()

    def prepare_next_file(self):
        if self.upcoming_file is None:
            # K, let's start with the first one.
            lf = self.log_file_factory.make(self.next_hour, self.batch_id)
            self.batch_id += 1
            self.next_hour += timedelta(hours=1)
            self.upcoming_file = lf
        else:
            print("[WRAN] cannot prepare next file: it is already set")
            return

    def next_batch(self, blocking=True):
        items = []
        while len(items) == 0:
            ids_to_delete = set()

            # Collect new items.
            for batch_id, lf in self.files.items():
                new_items = lf.consume()
                if len(new_items) > 0:
                    items.extend(new_items)
                elif not lf.expect_more():
                    print(f"[BATCH] planning to delete batch id: {batch_id}")
                    ids_to_delete.add(batch_id)

            # See if we need to open a new file.
            opened_new_file = False
            if len(items) == 0:
                if self.upcoming_file is None:
                    # Then set it!
                    self.prepare_next_file()
                if self.upcoming_file.exists():
                    if self.upcoming_file.open():
                        self.files[self.upcoming_file.batch_id] = self.upcoming_file
                        self.upcoming_file = None
                        opened_new_file = True
                    else:
                        print(f'[ERR] failed to open "{self.upcoming_file.filename}"')
                else:
                    file_hour = self.upcoming_file.hour
                    print(f"next file does not exist ({file_hour})")
                    current_time = datetime.now(tz=timezone.utc)
                    if current_time > (file_hour + SKIP_TIME):
                        # And we are past it's expected hour, will skip it.
                        print("[BATCH] skipping it")
                        self.upcoming_file = None
                        self.prepare_next_file()

            # Remove files we read completely.
            if len(ids_to_delete) > 0:
                for batch_id in ids_to_delete:
                    ts = self.files[batch_id].hour
                    lns = self.files[batch_id].lines_read
                    print(f"[BATCH] removing {batch_id} ({ts}, {lns} lines)")
                    num_files_before = len(self.files)
                    del self.files[batch_id]
                    err_msg = f"failed to delete file {batch_id}"
                    expected = len(self.files) + len(ids_to_delete)
                    assert num_files_before == expected, err_msg
                ids_to_delete.clear()

            # Wait for more if there is nothing to do at the moment.
            if len(items) == 0 and not opened_new_file:
                if blocking:
                    print(f"no new data, will sleep for {self.sleep_seconds}s")
                    sleep(self.sleep_seconds)
                else:
                    return []
        return items

    def next(self):
        yield self.next_batch()
