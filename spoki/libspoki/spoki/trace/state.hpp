/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2021
 * Authors: Raphael Hiesgen
 *
 * All rights reserved.
 *
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <caf/actor.hpp>
#include <caf/ipv4_address.hpp>
#include <caf/ipv4_subnet.hpp>
#include <caf/ref_counted.hpp>
#include <caf/scoped_actor.hpp>

#include "spoki/config.hpp"
#include "spoki/consistent_hash_map.hpp"
#include "spoki/hashing.hpp"
#include "spoki/task.hpp"

SPOKI_PUSH_WARNINGS
#include <libtrace_parallel.h>
SPOKI_POP_WARNINGS

namespace spoki::trace {

struct global;

/// Thread local storage for storing some statistics and a consistent hash map
/// to route data from the same source address to the same actor for processing.
struct local : caf::ref_counted {
  friend global;

  local(caf::actor_system& sys,
        const std::unordered_set<caf::ipv4_address>& filter,
        size_t batch_size);

  /// Process a new packet and extrace the information for probing the sender.
  void add_packet(libtrace_packet_t* packet);

  /// Force sending out all cached probes.
  void send_all();

  /// Pulish some stats as user results.
  void publish_stats(libtrace_t* trace, libtrace_thread_t* thread);

  // Data collected for libtrace results.
  uint64_t total_packets;
  uint64_t ipv4_packets;
  uint64_t ipv6_packets;
  uint64_t others;

private:
  caf::scoped_actor self;
  const size_t batch_size;
  std::unordered_map<caf::actor, std::vector<packet>> packets;
  caf::ipv4_subnet network;
  const std::unordered_set<caf::ipv4_address>& filter;
  // consistent_hash_map<caf::actor> router;
  std::vector<caf::actor> router;
  bool enable_filters;
};

/// Global state for libtrace processing
struct global {
  global(caf::actor_system& system, std::vector<caf::actor> shards,
         caf::actor parent, uint64_t id, size_t batch_size,
         std::unordered_set<caf::ipv4_address> filter);

  caf::actor_system& sys;
  std::vector<caf::actor> shards;
  const std::unordered_set<caf::ipv4_address> filter;
  caf::actor parent;
  const size_t batch_size;
  const uint64_t id;
  const int replication_factor = 100;

  caf::intrusive_ptr<local> make_local();
};

/// Structure that describes a result from a processing thread.
struct result {
  uint64_t total_packets = 0;
  uint64_t ipv4_packets = 0;
  uint64_t ipv6_packets = 0;
  uint64_t others = 0;
};

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, result& x) {
  return f.object(x).fields(f.field("total_packets", x.total_packets),
                            f.field("ipv4_packets", x.ipv4_packets),
                            f.field("ipv6_packets", x.ipv6_packets),
                            f.field("others", x.others));
}

/// State to collect results after libtrace processing.
struct tally {
  uint64_t total_packets = 0;
  uint64_t ipv4_packets = 0;
  uint64_t ipv6_packets = 0;
  uint64_t others = 0;
  uint64_t last_key = 0;
};

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, tally& x) {
  return f.object(x).fields(f.field("total_packets", x.total_packets),
                            f.field("ipv4_packets", x.ipv4_packets),
                            f.field("ipv6_packets", x.ipv6_packets),
                            f.field("pthers", x.others),
                            f.field("last_key", x.last_key));
}

} // namespace spoki::trace
