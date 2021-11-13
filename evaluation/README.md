# Malware analysis

CAF Spoki Evaluation, Version 2


## Dependencies

* Python3
* Python3 Virtualenvironments
* Kafka
  * Java

On Ubuntu 20.04.2 LTS, python3 is verison 3.8, here you can run:

```
$ sudo apt install python3.8-dev python3.8-venv openjdk-11-jdk
```


## Kafka

Malware processing uses Kafka for communication between processes. You can setup Kafka and Zookeeper as system services or just run them directly [as follows](https://kafka.apache.org/quickstart). (Since you need multiple processes running, `screen` or `tmux` can make the multiplexing easier.)


```
$ wget https://www.apache.org/dyn/closer.cgi?path=/kafka/3.0.0/kafka_2.13-3.0.0.tgz
$ tar -xzf kafka_2.13-3.0.0.tgz
$ cd kafka_2.13-3.0.0
$ # Start the ZooKeeper service in one tab.
$ bin/zookeeper-server-start.sh config/zookeeper.properties
# Start the Kafka broker service in another tab.
$ bin/kafka-server-start.sh config/server.properties
```


## Setup

Requires python 3. The development setup will link executables into the virtual environment and make them easily accessible for development.

```
$ # Setup a virtual environment.
$ python3 -m venv envs
$ source envs/bin/activate
$ # Now install the requires packges.
$ make update
$ # You might need to call this twice because the first run only install pip-tools.
$ make update
```


## Running

Once you got that running, the malware scripts can be started. They should each write (and read) from a Kafka topic. The tool chain requires quite a bit of memory and has not been optimized in that regard. 8GB of RAM should be safe.

*Note:*
- The date should be the date at which the processing starts.
- The hour should be the hour at which processing starts.
- The `LOG_DIR` is the folder with Spoki's logs.
- The programs `filter`, `clean`, and `download` can read from multiple topics and produce to a single new topic. `-c` sets a tag for the topic to consume from and `-p` sets the tag for the topic to produce to.

Example: ...

```
$ assemble -s 2021-08-16 -H 12 -d test -P 600 --kafka --csv LOG_DIR
$ filter -c test -p test
$ clean -c test -p test
$ download -c test
```

The script `reset-topics -d test` resets the data in the local Kafka instance for the tag `test`.


## Details

This lists details on the program options, the intermediate data formats, and how to process them.

### Assemble

`assemble` read the logs written by Spoki and assembles two-phase events from the events they contain. Spoki writes two types of logs:

1) Logs with the incoming packets and its reaction, i.e., if a probe was sent in response and what type of probe was sent,
2) Scamper logs that contain the details what kind of probe was sent at what time.

The events in both logs can be matched via a user id. In a first step, `assemble` matches the events from both logs. Then, it builds regular and irregular sequences (handshakes) from these events. A sequence would be either a regular or irregular SYN, the information how Spoki responded, and potentially and ACK, again with the information how Spoki responded.

These sequences can than be matched into two-phase events, i.e., an irregular sequence followed by a regular sequence. All events, two-phase, one phase, and unmatched packets are written to a sink by Spoki.

```
(envs) ~/cse2> assemble --help
Usage: assemble [OPTIONS] LOG_DIR

Options:
  -s, --start-date TEXT        process data starting at this day (YYYY-MM-DD)
                               [required]
  -H, --hour INTEGER           skip the first N hours of the first day
                               (default: 0)
  -d, --datasource TEXT        match phases for this datasource  [required]
  -t, --probe-timeout INTEGER  max interval between a req. and its conf. in
                               mins (default: 5)
  -P, --phase-timeout INTEGER  max interval between to phases in seconds
                               (default: 600)
  -e, --end-date TEXT          process data until this day (YYYY-MM-DD)
  -k, --kafka-port INTEGER     port of the local kafka server (default: 9092)
  --kafka-batch-size INTEGER   batch size for sending produced events
                               (default: 1000)
  -o, --output PATH            choose the output dir for non-kafka writer
                               (default: .)
  --swift / --local            toggle between reading data from local disk and
                               from OpenStack Swift (default: local)
  --kafka / --logs             toggle writing results to Kafka and to logs on
                               disk (default: logs)
  --compressed                 read gzip compressed files from disk
  --csv / --json               toggle between CSV and JSON input files
                               (default: CSV)
  --help                       Show this message and exit.
```

Spoki offers two kinds of sinks to write to: Kafka and log files on disk. This can be toggled with the `--kafka/--logs` options. Since the malware tool chain reads from Kafka this option is available for debug purposes and allows processing the data in different ways. When writing to logs the option `-o` chooses the folder to place the result logs into. For Kafka, the options `--kafka-port` set the port of the local Kafka instances and `--kafka-batch-size` configures how many events are buffered by kafka before forwarding them.

Similarly, the option `--swift/--local` configure `assemble` to read data from either OpenStack Swift or from disk. If data is stored in such a way for long-term storage, `assemble` can download and process log files directly from the backend. `--compressed` determines whether `assemble` should look for gzip compressed files (ending with .gz) or for uncompressed files (.csv or .json).

An earlier version of Spoki logged events as JSON instead CSV. The toggle `--csv/--json` tells `assemble` to look either for the newer CSV files or the earlier JSON files.

Two options configure the matching process. First, the option `--probe-timeout` sets the maximum time that is accepted between the receipt of a packet and the (potential) probe sent as a result. This helps to reduce the search complexity when correlating the two log types `assemble` ingests. Second, `phase-timeout` limits the time that is accepts between the irregular phase and the regular phase.

A starting point for processing logs can be chosen via the options `--start-date` and `--hour`. This is required. In contrast, choosing an time to stop processing (`--end-date`) is optional. When assemble reaches the current time it will regularly check for new data and eventually skip a log file if the next hour was reached and no log file is available.

The output is JSON formatted. The basic format is as follows:

```json
{
    "ts": UNIX_TS,
    "tag": "TYPE_STRING",
    "isyn": ...,
    "iack": ...,
    "rsyn": ...,
    "rack": ...,
}
```

The `ts` key has a Unix time stamp value. The `tag` marks the type of event: "isyn", "isyn (acked)", "rsyn", "rsyn (acked)", "two-phase","two-phase (no ack)". This tag signifies which of the fields are expected to have values. Those that don't have values are `null`. The keys that have a value maps to an object with two keys: `trigger` and `reaction`, see here:

```json
{
    "trigger": {
        "timestamp": UNIX_TS,
        "saddr": "IPv4",
        "daddr": "IPv4",
        "ipid": NUMBER,
        "ttl": NUMBER,
        "proto": "tcp",
        "payload": {
            "sport": PORT,
            "dport": PORT,
            "snum": NUMBER,
            "anum": NUMBER,
            "window_size": NUMBER,
            "flags": [
                "syn"
            ],
            "options": [
                "mss",
                "sack_permitted",
                "window_scale"
            ],
            "payload": ""
        }
    },
    "reaction": {
        "saddr": "IPv4",
        "daddr": "IPv4",
        "sport": PORT,
        "dport": PORT,
        "anum": NUMBER,
        "snum": NUMBER,
        "userid": NUMBER,
        "method": "tcp-synack",
        "num_probes": NUMBER,
        "payload": ""
    }
}

```

The `trigger` contains information about the originally received packet, including the timestamp of the receipt, the source and destination IPv4 addresses, the ipid and ttl from the IP header, and information about the enclosing formation, here "tcp". The `payload` key maps to information from the TCP header, i.e., source and destination ports, the windows size, a list of flags and a list of options (without the respective option value, this just signifies that these options were set in the packet), and finally a potential payload (HEX encoded).

The information for the `reaction` reflect the probe request sent to scamper. If no probe was requested the reaction can be `null`. If set, it contains the source and destination addresses as well as the source and destination port, the acknowledgement and sequence numbers, the user id (used to match the probe confirmation from scamper), the method (the type of probe), the number of probes that was requested and a potential payload included in the probe.

At runtime `assemble` prints information about the logs it attempts to read and events it matches. These are more statistics than detailed data.

These events can be read from the Kafka topic, e.g. using `kafka-python`.

```python
from kafka import KafkaProducer, KafkaConsumer

# This is the default Kafka port. Adjust it to your needs.
kafka_port = 9092

# This is the tag of the log files, i.e. "test" in the example above.
tag = "test"
kafka_consumer_topics = f"cse2.malware.events.{tag}"

# This group ID should be distinct from the group ID used by the malware toolchain.
kafka_group_id = "YOUR_GROUP_ID_HERE" 

print(f"consuming from '{kafka_consumer_topics}'")

consumer = KafkaConsumer(
    group_id=kafka_group_id,
    bootstrap_servers=[f"localhost:{kafka_port}"],
)
consumer.subscribe(kafka_consumer_topics)


while True:
    for msg in consumer:
        event = json.loads(msg.value.decode("utf-8"))
        # "event" is now a JSON object with the fields above.
```

### Filter

This program reads the events from `assemble` and filter the payloads in for occurrences of "wget" and "curl". For matching events it publishes a subset of the data along with the decoded payload to a new topic.
 
```
(envs) ~/cse2> filter --help
Usage: filter [OPTIONS]

Options:
  -c, --consume TEXT          match phases for this datasource  [required]
  -p, --produce TEXT          match phases for this datasource
  -k, --kafka-port INTEGER    port of the local kafka server (default: 9092)
  --kafka-batch-size INTEGER  batch size for sending produced events (default:
                              1)
  --help                      Show this message and exit.
```

The option `--consume` and `--produce` set a tag for the topics to read from and publish to. The consumed topic is matches this python format sting `f"cse2.malware.events.{tag}"` while the topic it publishes to is `"cse2.malware.filtered"` (if a produce tag is supplied it is append after a dot. `f"cse2.malware.filtered.{tag}"`).

Published events are JSON object with the following keys: "ts", "tag", "saddr", "daddr", "sport", "dport", "payload", "tool", "decoded". The `tag` in the events matches the tag in the consumed events (i.e., "isyn", "rsyn", "two-phase", ...). The `tool` is either "wget" or "curl", the `payload` is the hex-encoded payload string and `decoded` is the plain text version.

Every 100,000 events, `filter` prints the events it ingested and how many downloaders it identified.

### Clean

Clean extracts URLs from the events identified in the previous step. 

```
(envs) ~/cse2> clean --help
Usage: clean [OPTIONS]

Options:
  -c, --consume TEXT          match phases for this datasource
  -p, --produce TEXT          match phases for this datasource
  -k, --kafka-port INTEGER    port of the local kafka server (default: 9092)
  --kafka-batch-size INTEGER  batch size for sending produced events (default:
                              1)
  --log-name TEXT             Write error log for events that could not be
                              parsed
  --help                      Show this message and exit.
```

Similar to `filter` a tag for the topic to consume from and to publish to can be set. The consumed topic is `"cse2.malware.filtered"` with an optional tag at the end while the topic it publishes to is `"cse2.malware.cleaned"`, again with an optional tag at the end. An extra option `--log-name` sets the name for a log file with events where parsing the payload for a downloadable URL failed. Usually the payloads are not complete or contain unforeseen patterns.

At runtime, the script prints a line for each URL it identifies. It contains the tool, the URL, the hoster, and the port for the download, separated with '|'.

Events are published as JSON with "ts", "tag", "saddr", "daddr", "sport", "dport", "payload", "tool", "decoded", "url", "server", "port", 
"name". The last four fields are new. They contain the download URL and its parts separately (server, port, name).


### Download

The last part in the pipeline, `download` consumes the cleaned events and attempts to download executables from the given URLs.

```
(envs) ~/cse2> download --help
Usage: download [OPTIONS]

Options:
  -c, --consume TEXT        match phases for this datasource
  -k, --kafka-port INTEGER  port of the local kafka server (default: 9092)
  -w, --working-dir PATH    data directory for database and results (.)
  --help
```

Naturally, it reads from `"cse2.malware.cleaned"` with an optional tag at the end, set via `--consume`. The option `--working-dir` specifies the root directory for the information it collects. `download` creates two folders in its working directory:

* An `activity` folder contains logs for all discovered malware names.
* The folder `malware` collects the downloads sorted into subfolders that use the hashes of the downloaded data. Each of those folders contains the data named `malware.bin` and a compressed log file with meta data.

