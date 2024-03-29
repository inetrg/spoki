# Spoki: Unveiling a New Wave of Scanners through a Reactive Network Telescope

This folder contains artifacts for the paper:

```
Spoki: Unveiling a New Wave of Scanners through a Reactive Network Telescope.
R. Hiesgen, M. Nawrocki, A. King, A. Dainotti, T. C. Schmidt, and Matthias Wählisch.
Proc. of 31st USENIX Security Symposium, 2022, Berkeley, CA, USA.
```


## Structure

We included two parts in this artifact:

1. *Spoki*, a reactive telescope,
2. A subset of the *evaluation* tools we used for the paper.
  - Includes the *malware* tool chain to find links to executables in payloads.

Please check the respective README files for details.

### Spoki

Located in the folder `spoki` is a C++ program that listens on data source (such as an interface) for incoming packets. It answers SYNs to establish TCP connections and collects the payloads.

## Evaluation

Located in the folder "evaluation" is a set of python programs that process the Spoki logs to reassemble handshakes and perform some analysis. This includes the *malware tool chain* to identify payloads that contain downloaders. Additionally, this folder contains a small test program that can be run on a separate host to perform a two-phase handshake with Spoki.

### Scripts

This folder contains two scripts to drop outgoing TCP RST,ACK packets. Since you likely don't have a telescope setup your host will respond to incoming SYNs with a RST,ACK and interfere with Spoki. One of the scripts sets an iptables rule to DROP these outgoing packets and the other script deletes the rule.


## Test Setup

Spoki and the its tools create new data. We suggest using the following folder structure:

```
./
  logs       # unused
    example  # example logs
  evaluation # tools that process Spoki's logs
    data     # unused
  README.md  # this readme
  scripts    # helpful scripts
  spoki      # spoki's code
```

So far the folders `logs` and `evaluation/data` are empty. The first one can be used to collect Spoki logs and the second one to collect the malware data.

### Requirements

Detailed requirements are listed in the respected `README.md` files, here is a short overview:

* Linux system (e.g., Ubuntu 20.04)
* git
* C++ 17 compatible compiler (e.g., gcc 9.3)
* CMake >= 3.5
* make
* curl
* Python3 >= 3.6 (e.g. Python 3.8)
* Kafka

Spoki has a few more dependencies. A few can be installed via the OS dependency manager, see `README.md` of Spoki. The remaining libraries can build via the `setup.sh` script located in the `spoki` folder. At least scamper must be build this way because we apply a few patches to the code.

The basic steps to start Kafka are outlined in the `README.md` in the `malware` folder.

### Steps

A complete setup consists of Spoki and four python programs that process its logs.

1. Build Spoki.
2. Configure Spoki.
3. Run Spoki.
4. Run Zookeeper & Kafka.
6. Setup evaluation project.
7. Run the four malware processes.
8. Probe Spoki.

The python tools can be set up before Spoki as well. There are no direct dependencies aside from the logs produced by Spoki.


## Walkthrough

The complete setup requires running multiple processes in parallel. We suggest using `screen` or `tmux` to keep an overview.

### 1. Building Spoki

Please check the Spoki `README.md` file for details. On Ubuntu 20.04 the following should work:

```
$ sudo apt install gcc libtool-bin automake libpcap0.8-dev
$ ./setup.sh
$ ./configure
$ make -C build
```

### 2. Configuring Spoki

Spoki reads a `caf-application.conf` file from the directory it runs in (see `--long-help` on how to select a specific file). The `spoki` folder includes an example configuration file. You will need to adjust the `uri` to match your local setup, likely the interface to read data from, e.g., `int:eth0`.

Spoki needs a running scamper instance to use for probing. It can be started in a separate tab as follows. Add the IP and port to the TCP probers in the config file.
```
$ sudo ./scamper/scamper/scamper -p 10000 -P 127.0.0.1:11011
```

To run Spoki as a non-root user (recommended) you can give it the capabilities to read from the interface as follows:
```
$ sudo setcap cap_net_raw,cap_net_admin=eip ./build/tools/spoki
```

### 3. Running Spoki

Now, this should be as simple as `./build/tools/spoki` (executed in the `spoki` folder). Spoki prints a bit of configuration output. Thereafter it prints the following stats every second: *requests per second* (rps), *probes per second* (pps, rounded to 100), and a *queue size* (pending probes). When run on a single address pps mostly stays at 0 while rps should show occasional packets.

Information about buffer rotations interleave this output. The buffer sizes can be configured via two options in the collectors category. The example config file uses small values to ensure data is written quickly at low packet rates.

**NOTE** If you probe Spoki you should block outgoing RST,ACK packets, e.g., via `iptables`. The folder `scripts` has two scripts to "disable" and "enable" these outgoing packets.


### 4. Running Zookeeper & Kafka

The [quick start](https://kafka.apache.org/quickstart) guide suggest to run Kafka as follows. If you have an existing setup, feel free to use it.


```
$ wget https://www.apache.org/dyn/closer.cgi?path=/kafka/3.0.0/kafka_2.13-3.0.0.tgz
$ tar -xzf kafka_2.13-3.0.0.tgz
$ cd kafka_2.13-3.0.0
$ # Start the ZooKeeper service in one tab.
$ bin/zookeeper-server-start.sh config/zookeeper.properties
# Start the Kafka broker service in another tab.
$ bin/kafka-server-start.sh config/server.properties
```

*Note:* These are java tools. On Ubuntu 20.04 java can be installed as follows: `sudo apt install openjdk-11-jdk`.


### 6. Setup Evaluation Project

This is best done inside the `evaluation` folder. Since there are a few dependencies, we use a virtual environment for the setup. The required module can be install as follows on Ubuntu 20-.4: `sudo apt install python3.8-venv`. Then, you can setup the project as follows:

```
$ # Setup a virtual environment.
$ python3 -m venv envs
$ source envs/bin/activate
$ # Now install the requires packges.
$ make update
$ # You will have run this twice because the first run only install pip-tools.
$ make update
```

### 7. Running the Malware Tools

There are four tools:
1. `assemble` reads Spoki logs and assembles handshakes and phases.
2. `filter` reads assembled events from a Kafka topic, filters them, and forwards those that include "wget" or "curl" in their payload to a new topic.
3. `clean` reads the filtered events from Kafka, extracts the URLs, and writes the info to a new topic.
4. `download` reads the cleaned events and downloads whatever it finds behind the URLs.
Finally, `reset-topics` can reset the topics in the local Kafka instance. For details about the arguments each tool accepts and the intermediate formats, please see `malware/README.md`.

These tools run continuously in parallel. `assemble` requires a few arguments for configuration:
- The data and time of the logs to start with. The Spoki logs contain a date and hour in their name. If you already have logs, you can check those. The time is in UTC, usually `date` will tell you the current time in the shell.
- `-d` sets a tag. This tag needs to match the tag in the log files as well as the "consumer" tag of `filter`. The provided configuration file uses `test` as a tag.
- `--kafka` and `--csv` configure the program ingest CSV logs and to write to Kafka.
- The final (positional) argument is the folder that contains the logs.
`assemble` has quite the verbose output such as the files it tries to access, when it goes to sleep, etc. Don't be surprised if it quickly fills your screen.

Both, `filter` and `clean` need arguments for the tag to read from and write to. In this case this can simply be `test` as well. These are only tags to adapt the topic names. Expect little output from `filter`. `clean` prints the URLs it finds alongside a bit of metadata.

`download` needs a tag to read from and produces a variety of files--when data makes it through the pipeline. An `activity` folder contains logs for all discovered malware names. The folder `malware` collects the downloads sorted into subfolders that use the hashes of the downloaded data. Each of those folders contains the data named `malware.bin` and a compressed log file with meta data. Since this program creates new folders, we suggest running it inside a separate folder, e.g. `malware/data`.

(Don't forget to activate the virtual environment in each tab/terminal that runs one of the tools: `source envs/bin/activate`.)

```
$ assemble -s 2021-08-16 -H 12 -d test --kafka --csv ../logs
$ filter -c test -p test
$ clean -c test -p test
$ download -c test
```

The script `reset-topics -d test` resets the data in the local Kafka instance for the tag `test`.


### 8. Testing: Probing Spoki and the Malware Tools

The malware tools contain an additional script to probe Spoki with a two-phase event: `test-spoki`. It uses scapy to craft packets and simulate an interaction. You can run it from a separate host and probe the interface Spoki is listening on. The only required argument for `test-spoki` is the target host as a positional argument, although the port can be configured using `--port`.

The script blocks until it receives a reply from Spoki. When finished it should print DONE on the screen.

While interaction with Spoki is instant, Spoki does not flush the write buffer regularly and it might take a bit to write the logs to disk. You can check that the events show up in the log, e.g., by grep'ing for the start of the sequence number the tool uses for the packets. `12981`. The log that contains the events matches the pattern `YYYY-MM-DD.HH:00:00.TAG.spoki.tcp.raw.TIMESTAMP.csv`. Here is an example from my local setup (with an added header):

```
$ cat 2021-10-13.14:00:00.test.spoki.tcp.raw.1634133600.csv | grep 12981
ts|saddr|daddr|ipid|ttl|proto|sport|dport|anum|snum|options|payload|syn|ack|rst|fin|window size|probed|method|userid|probe anum|probe snum|num probes
1634135138255|141.22.28.35|141.22.28.17|61602|231|tcp|22734|42|0|1298127|||1|0|0|0|8192|true|tcp-synack|134217737|1298128|0|1
1634135138265|141.22.28.35|141.22.28.17|20202|64|tcp|41725|42|0|1298130|sack_permitted:timestamp:window_scale:mss||1|0|0|0|8192|true|tcp-synack|134217738|1298131|0|1
1634135138275|141.22.28.35|141.22.28.17|11727|64|tcp|41725|42|1908621218|1298131||7767657420687474703a2f2f3134312e32322e32382e33352f6576696c|0|1|0|0|8192|true|tcp-rst|134217739|0|1908621218|2
```

The malware tool chain takes quite a while. It wants to ensure that it doesn't miss delayed packets and thus waits up to an hour before writing events to disk. To shorten we wait time there are prepared example logs in `logs/example/`. They were created with the same tools in our lab environment. Pointing the `assassemble` to them will process the events quickly. These logs use the tag `test`. Start Kafka and then `filter`, `clean`, and `download` as noted above and run:

```
$ assemble -s 2021-10-13 -H 21 -d test --kafka --csv ../logs/example
```

`clean` should print a single line with the tool (wget), url (http://141.22.28.35/evil), hoster (141.22.28.35), and port (80). `download` will create a new `activity` folder with a log file for this malware name (`evil.2021.10.csv.gz`) that list the download attempt. If the download was successful, a directory `malware` will contain a folder named after the download hash that contains the download (`malware.bin`) and a compressed log file.

**Note** The download part accesses a file on my work computer. It get's multiple request from different IPs each day. I don't check the logs to see where from. Please let me know if I should disable it.

## More Tools

For more tools, please check the `README.md` in `evaluation`.

