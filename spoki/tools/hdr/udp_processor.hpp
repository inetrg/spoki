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

#pragma once

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <caf/all.hpp>
#include <caf/ipv4_address.hpp>

#include "spoki/atoms.hpp"
#include "spoki/hashing.hpp"

namespace up {

struct endpoint;

} // namespace up

CAF_BEGIN_TYPE_ID_BLOCK(spoki_tools_udp_processor, spoki::detail::spoki_core_end)

CAF_ADD_TYPE_ID(spoki_tools_udp_processor, (up::endpoint))
CAF_ADD_TYPE_ID(spoki_tools_udp_processor, (std::vector<up::endpoint>))

CAF_END_TYPE_ID_BLOCK(spoki_tools_udp_processor)

namespace up {

struct endpoint {
  caf::ipv4_address saddr;
  caf::ipv4_address daddr;
  uint16_t sport;
  uint16_t dport;
  std::vector<char> payload;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, endpoint& e) {
  return f(e.saddr, e.daddr, e.sport, e.dport, e.payload);
}

} // namespace up

namespace std {

template <>
struct hash<up::endpoint> {
  size_t operator()(const up::endpoint& ep) const {
    size_t seed = 0;
    spoki::hash_combine(seed, ep.dport);
    spoki::hash_combine(seed, ep.sport);
    spoki::hash_combine(seed, ep.daddr.bits());
    spoki::hash_combine(seed, ep.saddr.bits());
    // spoki::hash_combine(seed, ep.payload);
    return seed;
  }
};

template <>
struct equal_to<up::endpoint> {
  bool operator()(const up::endpoint& lhs, const up::endpoint& rhs) const {
    return lhs.saddr == rhs.saddr && lhs.daddr == rhs.daddr
           && lhs.sport == rhs.sport && lhs.dport == rhs.dport;
  }
};

} // namespace std

namespace up {

using clock = std::chrono::system_clock;
using time_point = clock::time_point;

// UDP-processor state.
struct ups {
  ups(caf::event_based_actor* self);
  ~ups();

  // Request more targets if only a few are in progress.
  void request_more();
  void start_probes();
  void start_probe(caf::ipv4_address addr);
  void retransmit(caf::ipv4_address addr);
  void new_target(endpoint& ep);

  void next_out_file();
  void append(caf::ipv4_address addr, uint16_t port, caf::string_view proto,
              const std::vector<char>& payload = {});

  std::unordered_map<caf::ipv4_address, std::deque<endpoint>> pending;
  std::unordered_map<caf::ipv4_address, std::pair<endpoint, time_point>>
    in_progress;
  std::unordered_map<endpoint, bool> finished;

  bool asked;
  caf::ipv4_address host;
  caf::actor source;
  caf::actor prober;

  // File handle to write to.
  std::ofstream out;

  static const char* name;
  caf::event_based_actor* self;
};

caf::behavior udp_processor(caf::stateful_actor<ups>* self, caf::actor prober,
                            caf::actor source, caf::ipv4_address host);

} // namespace up
