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

#include <caf/meta/type_name.hpp>
#include <caf/variant.hpp>

#include <nlohmann/json_fwd.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"
#include "spoki/net/five_tuple.hpp"
#include "spoki/net/icmp.hpp"
#include "spoki/net/icmp_type.hpp"
#include "spoki/net/tcp.hpp"
#include "spoki/net/udp.hpp"
#include "spoki/target_key.hpp"
#include "spoki/time.hpp"

namespace spoki {

struct SPOKI_CORE_EXPORT packet {
  using data = caf::variant<net::icmp, net::tcp, net::udp>;

  bool carries_icmp() const {
    return caf::holds_alternative<net::icmp>(proto);
  }

  bool carries_tcp() const {
    return caf::holds_alternative<net::tcp>(proto);
  }

  bool carries_udp() const {
    return caf::holds_alternative<net::udp>(proto);
  }

  template <class Proto>
  const Proto& protocol() const {
    return caf::get<Proto>(proto);
  }

  template <class Proto>
  Proto& protocol() {
    return caf::get<Proto>(proto);
  }

  std::time_t unix_ts() const;

  target_key get_key() const;

  net::five_tuple five_tuple() const;

  caf::ipv4_address saddr;
  caf::ipv4_address daddr;
  uint16_t ipid;
  uint8_t ttl;
  timestamp observed;
  data proto;
};

/// Equality comparison operator for packet.
SPOKI_CORE_EXPORT inline bool operator==(const packet& lhs, const packet& rhs) {
  return lhs.saddr == rhs.saddr && lhs.daddr == rhs.daddr
         && lhs.ipid == rhs.ipid && lhs.ttl == rhs.ttl
         && lhs.proto == rhs.proto;
}

/// Print a received packet.
SPOKI_CORE_EXPORT std::string to_string(const packet& x);

/// Get the protocol enum value.
struct vis_stringify {
  template <class T>
  std::string operator()(const T& x) const {
    return to_string(x);
  }
};

/// Get the protocol enum value.
struct vis_protocol_type {
  spoki::net::protocol operator()(const spoki::net::icmp&) const {
    return spoki::net::protocol::icmp;
  }
  spoki::net::protocol operator()(const spoki::net::tcp&) const {
    return spoki::net::protocol::tcp;
  }
  spoki::net::protocol operator()(const spoki::net::udp&) const {
    return spoki::net::protocol::udp;
  }
};

// -- Source address + protocol hash and comparison for unordered_map / set-----

/// Get distinct values to use for hashing.
struct hash_id_type {
  size_t operator()(const spoki::net::icmp&) const {
    return 1111;
  }
  size_t operator()(const spoki::net::tcp&) const {
    return 2222;
  }
  size_t operator()(const spoki::net::udp&) const {
    return 3333;
  }
};

// Add protocol information to hash.
struct vis_protocol_hash {
  void operator()(const spoki::net::icmp&) const {
    // nop
  }
  void operator()(const spoki::net::tcp& x) const {
    spoki::hash_combine(seed, x.sport);
    spoki::hash_combine(seed, x.dport);
  }
  void operator()(const spoki::net::udp& x) const {
    spoki::hash_combine(seed, x.sport);
    spoki::hash_combine(seed, x.dport);
  }
  size_t& seed;
};

template <class T>
struct probe_key_hash;

template <>
struct probe_key_hash<packet> {
  size_t operator()(const packet& x) const noexcept {
    size_t seed = 0;
    spoki::hash_combine(seed, caf::visit(hash_id_type{}, x.proto));
    spoki::hash_combine(seed, x.saddr);
    // TODO: Not sure if we should add this here. Not used atm.
    // spoki::hash_combine(seed, x.daddr);
    // caf::visit(vis_protocol_hash{seed}, x.proto);
    return seed;
  }
};

template <class T>
struct probe_key_equal_to;

template <>
struct probe_key_equal_to<packet> {
  bool operator()(const packet& lhs, const packet& rhs) const {
    vis_protocol_type vis;
    return lhs.saddr == rhs.saddr
           && caf::visit(vis, lhs.proto) == caf::visit(vis, rhs.proto);
  }
};

// -- JSON ---------------------------------------------------------------------

SPOKI_CORE_EXPORT void to_json(nlohmann::json& j, const packet& x);

SPOKI_CORE_EXPORT void from_json(const nlohmann::json&, packet&);

// -- Serialization ------------------------------------------------------------

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, packet& x) {
  return f.object(x).fields(f.field("saddr", x.saddr),
                            f.field("daddr", x.daddr), f.field("ipid", x.ipid),
                            f.field("ttl", x.ttl),
                            f.field("observed", x.observed),
                            f.field("proto", x.proto));
}

} // namespace spoki
