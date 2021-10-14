# Spoki

A reactive network telescope.

## Building Spoki

Spoki is written in C++ and depends on a variety of libraries.

### Dependencies

Build tools:

* Cmake
* C++17-compatible compiler

And the following libraries:

* [libtrace](https://research.wand.net.nz/software/libtrace.php)
* [nlohmann/json](https://github.com/nlohmann/json)
* [CAF](https://github.com/actor-framework/actor-framework)
* [libscamperfile](https://www.caida.org/catalog/software/scamper/) (comes with scamper)
* [libwandio](https://research.wand.net.nz/software/libwandio.php)

The `setup.sh` script downloads, builds, and installs them to `./deps`. (*Note:* Scamper will be patched by the setup script.)
Building the dependencies again requires a range of dependencies that can be installed via the system packet manager:

* GCC or clang
* libtool binary
* automake
* libpcap (>= 0.8)
* zlib

Ob Ubuntu you can run:
```
$ sudo apt install gcc libtool-bin automake libpcap0.8-dev
```

### Steps

You can build Spoki as follows. The `spoki` binary will be located in `./build/tools`.

```
$ ./setup.sh
$ ./configure
$ make -C build
```

## Configuration

Spoki uses a `caf-application.conf` file. Here's the outline for a conf file that would use two scamper instances to probe TCP. The URI for the data source must conform to the libtrace style. As an example, use `int:INTERFACE_NAME` to listen on an interface.

```
global {
  shards=1
  uri="DATA_SOURCE"
  live=true
  batch-size=1
  enable-filters=false
}

probers {
  tcp=[
    "HOST:PORT",
  ]
  unix-domain=false
  service-specific-probes=false
  reflect=true
}

cache {
  disable-udp=true
  disable-icmp=true
}

collectors {
  out-dir="PATH/TO/WRITE/LOGS/TO"
  datasource-tag="TAG_FOR_LOG_NAME"
}

caf {
  logger {
    verbosity='ERROR'
    file-name="out.log"
    inline-output=true
    console='COLORED'
    console-format="%c %p %a %t %C %M %F:%L %m"
  }
}
```

Start Spoki using the `spoki` executable with the conf file in the same folder or pass a specific config file using the `--config-file=PATH` CLI argument.

```
$ ./build/tools/spoki
```

If you want Spoki to read traffic directly from an interface as an unprivileged user the following command gives it the required capabilities.

```
$ sudo setcap cap_net_raw,cap_net_admin=eip ./build/tools/spoki
```

*NOTE: In it's current version Spoki requires one shard per scamper daemon it is managing. To start 20 using the port range 11001 to 11020 use the following. To higher numbers of scamper daemons it is recommended to connect via UNIX domain sockets. See the scamper help text and set `probers.unix-domain` to true in the Spoki conf file..*

```
$ for port in $(seq 11001 11020); do sudo ./deps/bin/scamper -p 20000 -P HOST:$port -D; done
```

## Developing

### Generating enum source files

I'm using the methodology from CAF to generate the source files and use implement the serialization. This gives us two new Cmake targets: one to check and to generate the files:

```
$ # Needs utility targets enabled.
$ ./configure --enable-utility-targets
$ # ... and then:
$ cmake --build ./build -t update-enum-strings
$ cmake --build ./build -t consistency-check
```

