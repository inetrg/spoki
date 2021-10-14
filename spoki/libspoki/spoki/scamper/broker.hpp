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
#include <deque>
#include <functional>
#include <set>
#include <string_view>
#include <vector>

#include <caf/all.hpp>
#include <caf/io/all.hpp>
#include <caf/io/broker.hpp>
#include <caf/ipv4_address.hpp>

#include "spoki/atoms.hpp"
#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"
#include "spoki/net/protocol.hpp"
#include "spoki/probe/payloads.hpp"
#include "spoki/probe/request.hpp"
#include "spoki/scamper/async_decoder.hpp"
#include "spoki/task.hpp"

namespace spoki::scamper {

// -- scamper communication state ----------------------------------------------

/// Track state for communication with a scamper daemon instance.
struct SPOKI_CORE_EXPORT instance {
  instance();

  /// Received, undecoded data.
  std::vector<caf::byte> buf;

  /// Number of bytes of expected data that needs to be decoded.
  size_t data_left;

  /// Track how many more requests we can send to scamper.
  uint32_t more;

  /// Conncetion info.
  std::string host;
  uint16_t port;

  /// Asnyconous warts decoder implementation, currently runs its own
  /// multiplexer loop.
  std::unique_ptr<async_decoder> dec;
};

/// Scamper broker state. This broker manages communicaton with a number of
/// scamper daemons, tasks them to probe targets, decodes the results and
/// returns them to the actor that initiated the event.
struct SPOKI_CORE_EXPORT broker_state {
  using conn = caf::io::connection_handle;

  // -- constructor and destructor ---------------------------------------------

  broker_state(caf::io::broker* self);
  ~broker_state();

  // -- scamper interaction ----------------------------------------------------

  /// Send as many request as we can. Tries to assign work in round robin.
  void send_requests();

  /// Send a request to a specific scamper instance connected via `hdl`.
  bool send_requests(conn hdl);

  /// Attach to a scamper daemon reachable via `hdl`.
  void attach(conn hdl);

  /// Detach from a scamper daemon reachable via `hdl`.
  void detach(conn hdl);

  // -- process replies --------------------------------------------------------

  /// Decode and process data in `buf` received via `hdl`.
  bool handle_data(conn hdl, const std::string_view& buf);

  /// Parse and handle a scamper reply in `buf` received via `hdl`.
  void handle_reply(conn hdl, const std::string_view& buf);

  // -- state ------------------------------------------------------------------

  /// Queued requests, not sent to a scamper yet.
  std::deque<probe::request> request_queue;

  /// All probes in progess, a superset of the requests queue and all requests
  /// delegated to scamper.
  std::unordered_map<uint32_t, caf::actor> in_progress;

  /// State for scamper daemons accessible through their connection handle.
  std::unordered_map<conn, instance> daemons;

  /// When sending out work, start with the daemon at `offset`.
  size_t offset;

  /// All connected scamper daemons.
  std::vector<conn> handles;

  /// Collector for scamper reply data as json.
  caf::actor reply_collector;

  /// Name that appears in logs, etc.
  static const char* name;

  /// Some stats.
  uint32_t stats_completed;
  uint32_t stats_more;
  uint32_t stats_new;
  uint32_t stats_requested;
  uint32_t stats_rst_completed;
  uint32_t stats_rst_new;
  uint32_t stats_rst_in_progress;
  std::unordered_map<conn, uint32_t> stats_more_per_broker;
  std::unordered_map<conn, uint32_t> stats_requested_per_broker;

  /// Default fields for packets
  uint32_t daddr_prefix = 0x0000010A;
  uint32_t daddr_suffix_max = 0x00feffff;
  uint32_t daddr = 1;
  uint32_t user_id_counter = 0;
  probe::request req;

  /// Pointer to the associated broker of this state.
  caf::io::broker* self;
};

/// Returns the beavior for a new scamper broker.
CAF_CORE_EXPORT caf::behavior
broker(caf::io::stateful_broker<broker_state>* self,
               const std::string& tag);

} // namespace spoki::scamper
