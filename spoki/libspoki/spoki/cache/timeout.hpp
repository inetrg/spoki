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

#include <functional>

#include "caf/ipv4_address.hpp"
#include "caf/variant.hpp"

#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"
#include "spoki/net/protocol.hpp"
#include "spoki/packet.hpp"

namespace spoki::cache {

struct tcp_timeout {
  caf::ipv4_address addr;
};

struct icmp_timeout {
  caf::ipv4_address addr;
};

struct udp_timeout {
  caf::ipv4_address addr;
};

using timeout = caf::variant<icmp_timeout, tcp_timeout, udp_timeout>;

// -- factory ------------------------------------------------------------------

timeout make_timeout(const packet& pkt);

// -- comparison ---------------------------------------------------------------

/// Equality comparison operator for tcp timeouts.
inline bool operator==(const tcp_timeout& lhs, const tcp_timeout& rhs) {
  return lhs.addr == rhs.addr;
}

/// Equality comparison operator for icmp timeouts.
inline bool operator==(const icmp_timeout& lhs, const icmp_timeout& rhs) {
  return lhs.addr == rhs.addr;
}

/// Equality comparison operator for udp timeouts.
inline bool operator==(const udp_timeout& lhs, const udp_timeout& rhs) {
  return lhs.addr == rhs.addr;
}


// -- serialization ------------------------------------------------------------

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, tcp_timeout& x) {
  return f.object(x).fields(f.field("addr", x.addr));
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, udp_timeout& x) {
  return f.object(x).fields(f.field("addr", x.addr));
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, icmp_timeout& x) {
  return f.object(x).fields(f.field("addr", x.addr));
}

// -- hashing ------------------------------------------------------------------

struct timeout_hash_visitor {
  template <class T>
  size_t operator()(const T& x) const noexcept {
    std::hash<T> h;
    return h(x);
  }
};

} // namespace spoki::cache

namespace std {

template <>
struct hash<spoki::cache::tcp_timeout> {
  size_t operator()(const spoki::cache::tcp_timeout& t) const noexcept {
    std::hash<caf::ipv4_address> h;
    return h(t.addr);
  }
};

template <>
struct hash<spoki::cache::icmp_timeout> {
  size_t operator()(const spoki::cache::icmp_timeout& t) const noexcept {
    std::hash<caf::ipv4_address> h;
    return h(t.addr);
  }
};

template <>
struct hash<spoki::cache::udp_timeout> {
  size_t operator()(const spoki::cache::udp_timeout& t) const noexcept {
    std::hash<caf::ipv4_address> h;
    return h(t.addr);
  }
};

template <>
struct hash<spoki::cache::timeout> {
  size_t operator()(const spoki::cache::timeout& t) const noexcept {
    return caf::visit(spoki::cache::timeout_hash_visitor(), t);
  }
};

} // namespace std
