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

#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"
#include "spoki/net/protocol.hpp"

#include <nlohmann/json_fwd.hpp>

#include <caf/fwd.hpp>

namespace spoki::net {

/// Five tuple for a transport packet. In case of ICMP, the ports will be 0.
struct SPOKI_CORE_EXPORT five_tuple {
  protocol proto;
  caf::ipv4_address saddr;
  caf::ipv4_address daddr;
  uint16_t sport;
  uint16_t dport;
};

/// Equality comparison operator for `five_tuple`.
inline bool operator==(const five_tuple& lhs, const five_tuple& rhs) {
  return lhs.proto == rhs.proto && lhs.saddr == rhs.saddr
         && lhs.daddr == rhs.daddr && lhs.sport == rhs.sport
         && lhs.dport == rhs.dport;
}

// -- JSON ---------------------------------------------------------------------

void to_json(nlohmann::json& j, const five_tuple& x);

void from_json(const nlohmann::json&, five_tuple&);

// -- CAF serialization --------------------------------------------------------

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, five_tuple& x) {
  return f.object(x).fields(f.field("proto", x.proto),
                            f.field("saddr", x.saddr),
                            f.field("daddr", x.daddr),
                            f.field("sport", x.sport),
                            f.field("dport", x.dport));
}

} // namespace spoki::net

namespace std {

template <>
struct hash<spoki::net::five_tuple> {
  size_t operator()(const spoki::net::five_tuple& x) const {
    size_t seed = 0;
    spoki::hash_combine(seed, x.proto);
    spoki::hash_combine(seed, x.saddr);
    spoki::hash_combine(seed, x.daddr);
    spoki::hash_combine(seed, x.sport);
    spoki::hash_combine(seed, x.dport);
    return seed;
  }
};

} // namespace std
