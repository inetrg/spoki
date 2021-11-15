#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
These classes write JSON data into a sink defined by their type.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import gzip
import json

from kafka import KafkaProducer
from pathlib import Path


class KafkaWriter:
    default_server = ["localhost:9092"]
    default_batch_size = 1000

    def __init__(self, topic, bootstrap_servers=None, batch_size=None) -> None:
        if bootstrap_servers is None:
            self.server = self.default_server
        else:
            self.server = bootstrap_servers

        if batch_size is None:
            self.batch_size = self.default_batch_size
        else:
            self.batch_size = batch_size

        self.producer = KafkaProducer(
            bootstrap_servers=self.server,
            batch_size=self.batch_size,
            value_serializer=lambda x: json.dumps(x).encode("utf-8"),
        )
        self.topic = topic

    def write_elems(self, elements, _):
        for elem in elements:
            self.producer.send(self.topic, elem)

    def write_elem(self, elem, _):
        self.producer.send(self.topic, elem)


class LogWriter:
    def __init__(self, datasource, path):
        self.datasource = datasource
        self.path = path
        self.openfiles = {}
        self.openfiletimestamps = []

    def __cleanup(self):
        if len(self.openfiletimestamps) > 2:
            print("[LW] cleaning up")
            for k, v in self.openfiles.items():
                print(f"[LW] {k} -> {v}")
            self.openfiletimestamps.sort()
            toremove = self.openfiletimestamps[0]
            print(f"[LW] closing'{toremove}'")
            fh = self.openfiles[toremove]
            fh.close()
            del self.openfiles[toremove]
            del self.openfiletimestamps[0]
        else:
            print("[LW] too few files to clean up")

    def __get_file(self, filets):
        if filets in self.openfiles:
            fh = self.openfiles[filets]
            return fh
        else:
            fn = f"{self.datasource}-events-{filets}.json.gz"
            if Path(fn).is_file():
                print(f"[LW] WARN: file '{fn}' already existed!")
            # err_msg = f"[LW] log file already exists: {fn}"
            # assert not Path(fn).is_file(), err_msg
            fh = gzip.open(fn, "at", newline="")
            self.openfiletimestamps.append(filets)
            self.openfiles[filets] = fh
            self.__cleanup()
            return fh

    def write_elems(self, elements, filets):
        fh = self.__get_file(filets)
        # json.dump(elements, fh)
        for elem in elements:
            fh.write(json.dumps(elem) + '\n')

    def write_elem(self, elem, filets):
        fh = self.__get_file(filets)
        # json.dump(elem, fh)
        fh.write(json.dumps(elem) + '\n')
