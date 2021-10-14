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

#include <caf/all.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/packet.hpp"

#include "spoki/net/endpoint.hpp"

namespace spoki::cache {

/// State for the shard actor which holds a cache to perform looks and trigger
/// probes if the cached data is not available.
struct SPOKI_CORE_EXPORT shard_state {
  // -- constructor and destructor ---------------------------------------------

  shard_state(caf::event_based_actor* self);

  ~shard_state();

  // -- member functions -------------------------------------------------------

  /// Get the tag for the next request.
  uint32_t next_id();

  /// Process a single packet.
  void handle_packet(const packet& pkt);

  // -- state ------------------------------------------------------------------

  /// Actor that collects the results and writes them to disk.
  caf::actor result_handler;
  caf::actor tcp_collector;
  caf::actor icmp_collector;
  caf::actor udp_collector;

  /// Associated prober for probing targets with scamper.
  caf::actor tcp_prober;
  caf::actor icmp_prober;
  caf::actor udp_prober;

  /// Disable some probing types.
  bool enable_icmp;
  bool enable_tcp;
  bool enable_udp;
  
  /// Reset cache.
  std::unordered_set<net::endpoint> rst_scheduled;

  /// Add tags to request to match them to scamper's replies.
  uint32_t shard_id; // Only use the upper  8 bit!
  uint32_t tag_cnt;  // Only use the lower 24 bit!

  /// Name that appears in logs, etc.
  static const char* name;

  /// Pointer to the actor that holds this state.
  caf::event_based_actor* self;
};

SPOKI_CORE_EXPORT caf::behavior
shard(caf::stateful_actor<shard_state>* self, caf::actor tcp_prober,
      caf::actor icmp_prober, caf::actor udp_prober);

} // namespace spoki::cache
