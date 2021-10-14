/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2019
 * Authors: Raphael Hiesgen
 *
 * All rights reserved.
 *
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#include <chrono>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <sstream>

#include <caf/all.hpp>
#include <caf/io/all.hpp>
#include <caf/string_algorithms.hpp>

#include "is_ip_address.hpp"

#include "spoki/all.hpp"
#include "spoki/cache/shard.hpp"
#include "spoki/collector.hpp"
#include "spoki/probe/udp_prober.hpp"
#include "spoki/scamper/broker.hpp"
#include "spoki/trace/processing.hpp"
#include "spoki/trace/reader.hpp"
#include "spoki/trace/reporting.hpp"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

auto subnet_from_config(const std::string& prefix)
  -> caf::optional<caf::ipv4_subnet> {
  std::vector<std::string> parts;
  caf::split(parts, prefix, "/");
  if (parts.size() != 2) {
    cout << "network should be formated IP/PREFIX_LENGTH" << std::endl;
    return none;
  }
  caf::ipv4_address addr;
  auto err = caf::parse(parts[0], addr);
  if (err) {
    cout << "cannot parse an IPv4 address from " << parts[0] << ":"
         << std::endl;
    //<< to_string(err) << std::endl;
    return none;
  }
  auto len = strtol(parts[1].c_str(), nullptr, 10);
  if (len <= 0 || len > 32) {
    cout << "not a valid prefix: " << parts[1] << std::endl;
    return none;
  }
  auto snet = caf::ipv4_subnet(addr, static_cast<uint8_t>(len));
  std::cout << "ignoring packets originating in our network or not "
               "addressed to it: '"
            << to_string(snet) << "' (configured via -n)" << std::endl;
  return snet;
};

// Converts a vector of HOST:PORT strings into a vector of (string, uint16_t)
// elements. Estimates if the host is a valid address.
auto parse_addrs(const std::vector<std::string>& addrs) {
  std::vector<std::pair<std::string, uint16_t>> results;
  if (addrs.empty())
    return results;
  caf::ipv4_address tmp;
  for (auto& tup : addrs) {
    std::vector<std::string> splits;
    caf::split(splits, tup, ":");
    if (splits.size() != 2 || !spoki::is_valid_host(splits.front())) {
      std::cout << "could not parse '" << tup << "' into address information, "
                << "expecting format 'HOST:PORT'" << std::endl;
      results.clear();
      return results;
    }
    auto& host = splits[0];
    auto port = static_cast<uint16_t>(std::stoi(splits[1]));
    results.emplace_back(make_pair(std::move(host), port));
  }
  return results;
};

class configuration : public caf::actor_system_config {
public:
  // Need initializion or have defaults elsewhere.
  string uri;
  string dir;
  std::vector<string> tcp_probers;
  std::vector<string> icmp_probers;
  std::vector<string> udp_probers;
  string filter;
  string network;
  string datasource_tag;

  // Used directly, need defaults here ...
  uint32_t shards = 2;
  uint32_t ingest_threads = 8;
  size_t batch_size = 1;

  bool use_unix_socket = false;
  bool reflect = false;
  bool ssp = false;
  bool ingest_stats = false;
  bool disable_icmp = false;
  bool disable_tcp = false;
  bool disable_udp = false;

  size_t reserve_size = spoki::buffer_reserve_mem;
  size_t write_threshold = spoki::buffer_send_mem;

  configuration() {
    opt_group{custom_options_, "global"}
      .add(shards, "shards,s", "set number of shards (1)")
      .add(uri, "uri,u", "set libtrace uri")
      .add(ingest_threads, "ingets-threads,t",
           "number of threads for packet ingestion (8)")
      .add(ingest_stats, "ingest-state,i", "print libtrace stats")
      .add(batch_size, "batch-size,b",
           "set batch size for forwarding incoming packets (1)")
      .add(filter, "filter,f", "drop packets form these IPs")
      .add(network, "network,n", "set network we work with");
    opt_group{custom_options_, "cache"}
      .add(disable_icmp, "disable-icmp,X", "disable IMCP probing")
      .add(disable_tcp, "disable-tcp,Y", "disable TCP probing")
      .add(disable_udp, "disable-udp,Z", "disable UDP probing");
    opt_group{custom_options_, "probers"}
      .add(tcp_probers, "tcp,T", "set scamper probing daemons (TCP)")
      .add(icmp_probers, "icmp,I", "set scamper probing daemons (ICMP)")
      .add(udp_probers, "udp,U", "set scamper probing daemons (UDP)")
      .add(use_unix_socket, "unix-domain,D", "use unix domain sockets")
      .add(reflect, "reflect,r", "reflect payload for UDP probing")
      .add(ssp, "service-specific-probes,S",
           "send service-specific probes when available");
    opt_group{custom_options_, "collectors"}
      .add(dir, "out-dir,d", "set result log directory")
      .add(datasource_tag, "datasource-tag,t",
           "set datasource tag used in the outputfile names")
      .add(reserve_size, "reserve-size,M",
           "reserved size for each write buffer (in bytes)")
      .add(write_threshold, "write-threshold,N",
           "threashold to forward buffer for writing (in bytes)");
    // nop
  }
};

void caf_main(actor_system& system, const configuration& config) {
  using addr_vec = std::vector<std::pair<std::string, uint16_t>>;

  if (config.uri.empty()) {
    std::cerr << "please specify an URI for libtrace input using '-u'.\n";
    return;
  }

  {
    if (caf::get_or(system.config(), "enable-filters", false)) {
      // Check if this is valid input.
      auto prefix = caf::get_or(config, "network", "127.0.0.1/32");
      auto host = subnet_from_config(prefix);
      if (!host)
        return;
    }
  }
  auto brokers_supplied = !config.tcp_probers.empty()
                          || !config.icmp_probers.empty()
                          || !config.udp_probers.empty();
  std::vector<std::pair<std::string, uint16_t>> tcp_probers;
  std::vector<std::pair<std::string, uint16_t>> icmp_probers;
  std::vector<std::pair<std::string, uint16_t>> udp_probers;
  if (!config.use_unix_socket) {
    tcp_probers = parse_addrs(config.tcp_probers);
    icmp_probers = parse_addrs(config.icmp_probers);
    udp_probers = parse_addrs(config.udp_probers);
    brokers_supplied = !tcp_probers.empty() || !icmp_probers.empty()
                       || !udp_probers.empty();
  }
  if (!brokers_supplied) {
    std::cout << "please provide at scamper daemon for probing"
              << ", see --help for details" << std::endl;
    return;
  }
  // Build filter set from address list.
  std::unordered_set<caf::ipv4_address> filter;
  if (!config.filter.empty()) {
    std::vector<std::string> parts;
    caf::split(parts, config.filter, ",");
    for (auto& str : parts) {
      caf::ipv4_address addr;
      auto res = caf::parse(str, addr);
      if (res) {
        std::cout << "failed to parse '" << str
                  << "' into an IPv4 address to filter" << std::endl;
        return;
      }
      filter.insert(addr);
    }
  }
  // A scope for the scoped actor.
  {
    scoped_actor self{system};
    std::vector<caf::actor> shards;
    std::vector<caf::actor> brokers;
    std::vector<caf::actor> tcp_managers;
    bool experienced_failure = false;

    // -- probing --------------------------------------------------------------

    if ((config.use_unix_socket && config.shards > config.tcp_probers.size())
        || (!config.use_unix_socket && config.shards > tcp_probers.size())) {
      aout(self) << "need one prober per shard\n";
      return;
    }
    // TODO: Create managers for ICMP and UDP
    caf::actor icmp_manager;
    caf::actor udp_manager;

    // -- core -----------------------------------------------------------------

    aout(self) << "creating " << config.shards << " shards\n";
    for (uint32_t i = 0; i < config.shards; ++i) {
      if (config.use_unix_socket) {
        auto& name = config.tcp_probers[i];
        auto tcp_manager = system.spawn(spoki::scamper::manager_unix, "tcp",
                                        name);
        shards.emplace_back(system.spawn(spoki::cache::shard, tcp_manager,
                                         icmp_manager, udp_manager));
        tcp_managers.emplace_back(tcp_manager);
      } else {
        auto& [host, port] = tcp_probers[i];
        auto tcp_manager = system.spawn(spoki::scamper::manager, "tcp", host,
                                        port);
        shards.emplace_back(system.spawn(spoki::cache::shard, tcp_manager,
                                         icmp_manager, udp_manager));
        tcp_managers.emplace_back(tcp_manager);
      }
    }

    // -- I/O ------------------------------------------------------------------

    if (!config.dir.empty()) {
      // Start collectors for
      auto tcp_collector = system.spawn<caf::detached>(
        spoki::collector, config.dir, "raw", "tcp",
        std::string{spoki::defaults::raw_csv_header}, 54321u);
      auto icmp_collector
        = system.spawn(spoki::collector, config.dir, "raw", "icmp",
                       std::string{spoki::defaults::raw_csv_header}, 54322u);
      auto udp_collector
        = system.spawn(spoki::collector, config.dir, "raw", "udp",
                       std::string{spoki::defaults::raw_csv_header}, 54323u);

      auto rs = config.reserve_size;
      auto wt = config.write_threshold;

      // We see little UDP and ICMP compared to TCP.
      auto icmp_buffer = system.spawn(spoki::buffer, icmp_collector, rs, wt);
      auto udp_buffer = system.spawn(spoki::buffer, udp_collector, rs, wt);

      // Send protocol-specific collectors to all shards.
      for (auto& s : shards) {
        self->send(s, spoki::collect_tcp_atom_v, spoki::start_atom_v,
                   system.spawn<caf::detached>(spoki::buffer, tcp_collector, rs,
                                               wt));
        self->send(s, spoki::collect_icmp_atom_v, spoki::start_atom_v,
                   icmp_buffer);
        self->send(s, spoki::collect_udp_atom_v, spoki::start_atom_v,
                   udp_buffer);
      }

      // Now for scamper results.
      auto scamper_tcp_collector = system.spawn<caf::detached>(
        spoki::collector, config.dir, "scamper", "tcp",
        std::string{spoki::defaults::scamper_csv_header}, 54324u);
      auto scamper_icmp_collector
        = system.spawn(spoki::collector, config.dir, "scamper", "icmp",
                       std::string{spoki::defaults::scamper_csv_header},
                       54325u);
      auto scamper_udp_collector
        = system.spawn(spoki::collector, config.dir, "scamper", "udp",
                       std::string{spoki::defaults::scamper_csv_header},
                       54326u);

      // We see little UDP and ICMP compared to TCP.
      self->send(icmp_manager, spoki::collect_atom_v, spoki::start_atom_v,
                 system.spawn(spoki::buffer, scamper_icmp_collector, rs, wt));
      self->send(udp_manager, spoki::collect_atom_v, spoki::start_atom_v,
                 system.spawn(spoki::buffer, scamper_udp_collector, rs, wt));

      for (auto& mgr : tcp_managers) {
        self->send(mgr, spoki::collect_atom_v, spoki::start_atom_v,
                   system.spawn<caf::detached>(spoki::buffer,
                                               scamper_tcp_collector, rs, wt));
      }
    }

    // -- ingestion ------------------------------------------------------------

    auto reader = system.spawn(spoki::trace::reader_with_filter, shards,
                               std::move(filter));

    // And send the URI for reading.
    aout(self) << "starting libtrace reader with " << config.ingest_threads
               << " threads\n";
    aout(self) << "will read from '" << config.uri << "'\n";
    self
      ->request(reader, std::chrono::seconds(5), spoki::trace_atom_v,
                config.uri, config.ingest_threads, config.batch_size)
      .receive(
        [&](spoki::success_atom) {
          // good, let's collect some stats
          // if (!config.dir.empty())
          //   self->send(reader, spoki::stats_atom_v, spoki::start_atom_v,
          //              config.dir);
        },
        [&](error& err) {
          aout(self) << "starting libtrace reader failed: " << to_string(err)
                     << endl;
          experienced_failure = true;
        });
    if (config.ingest_stats) {
      self->send(reader, spoki::stats_atom_v, spoki::start_atom_v);
      std::cout << "requesting stats\n";
    }
    std::cout << "should be up and running" << std::endl;

    // -- check and wait -------------------------------------------------------

    if (experienced_failure) {
      aout(self) << "experienced failure during startup\n";
      for (auto& a : shards) {
        self->send_exit(a, caf::exit_reason::user_shutdown);
      }
      for (auto& a : brokers) {
        self->send_exit(a, caf::exit_reason::user_shutdown);
      }
      for (auto& a : tcp_managers) {
        self->send_exit(a, caf::exit_reason::user_shutdown);
      }
      self->send(reader, caf::exit_reason::user_shutdown);
    } else {
      // Only crash if all actors shut down.
      system.await_all_actors_done();
      aout(self) << "WARN: shutting down unexpectedly" << std::endl;
    }
  }
  system.await_all_actors_done();
}

} // namespace

CAF_MAIN(id_block::spoki_core, ::io::middleman)
