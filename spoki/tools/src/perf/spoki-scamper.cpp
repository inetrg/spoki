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
  std::vector<string> tcp_probers;

  // Used directly, need defaults here ...
  uint32_t shards = 1;
  uint32_t ingest_threads = 8;
  size_t batch_size = 1;
  bool disable_icmp = true;
  bool disable_tcp = false;
  bool disable_udp = true;

  configuration() {
    opt_group{custom_options_, "global"}
      .add(shards, "shards,s", "set number of shards (1)")
      .add(uri, "uri,u", "set libtrace uri")
      .add(ingest_threads, "ingets-threads,t",
           "number of threads for packet ingestion (8)")
      .add(batch_size, "batch-size,b",
           "set batch size for forwarding incoming packets (1)");
    opt_group{custom_options_, "probers"}
      .add(tcp_probers, "tcp,T", "set scamper probing daemons (TCP)");
  }
};

void caf_main(actor_system& system, const configuration& config) {
  using addr_vec = std::vector<std::pair<std::string, uint16_t>>;

  if (config.uri.empty()) {
    std::cerr << "please specify an URI for libtrace input using '-u'.\n";
    return;
  }

  auto tcp_probers = parse_addrs(config.tcp_probers);
  if (!tcp_probers.empty()) {
    std::cout << "please provide at scamper daemon for probing"
              << ", see --help for details" << std::endl;
    return;
  }
  // A scope for the scoped actor.
  {
    scoped_actor self{system};
    std::vector<caf::actor> shards;
    std::vector<caf::actor> brokers;
    bool experienced_failure = false;

    // -- create brokers -------------------------------------------------------
    if (config.shards > tcp_probers.size()) {
      aout(self) << "need one prober per shard\n";
      return;
    }
    // Dummy managers that won't be used.
    caf::actor icmp_manager;
    caf::actor udp_manager;

    // -- create shards --------------------------------------------------------
    aout(self) << "creating " << config.shards << " shards\n";
    for (uint32_t i = 0; i < config.shards; ++i) {
      auto& [host, port] = tcp_probers[i];
      auto tcp_manager = system.spawn<caf::detached>(spoki::scamper::manager,
                                                     "tcp", host, port);
      shards.emplace_back(system.spawn(spoki::cache::shard, tcp_manager,
                                       icmp_manager, udp_manager));
    }

    // -- create trace reader --------------------------------------------------
    auto reader = system.spawn(spoki::trace::reader, shards);

    // And send the URI for reading.
    aout(self) << "starting libtrace reader with " << config.ingest_threads
               << " threads\n";
    aout(self) << "will read from '" << config.uri << "'\n";
    self
      ->request(reader, std::chrono::seconds(5), spoki::trace_atom_v,
                config.uri, config.ingest_threads, config.batch_size)
      .receive(
        [&](spoki::success_atom) {
          // Good.
        },
        [&](error& err) {
          aout(self) << "starting libtrace reader failed: " << to_string(err)
                     << endl;
          experienced_failure = true;
        });
    std::cout << "should be up and running" << std::endl;

    // -- wait for everything to finish ----------------------------------------
    if (experienced_failure) {
      aout(self) << "experienced failure during startup\n";
      for (auto& a : shards) {
        self->send_exit(a, caf::exit_reason::user_shutdown);
      }
      for (auto& a : brokers) {
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
