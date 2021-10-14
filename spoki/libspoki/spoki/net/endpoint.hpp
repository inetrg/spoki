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

#include <caf/ipv4_address.hpp>

#include "spoki/hashing.hpp"

namespace spoki::net {

struct SPOKI_CORE_EXPORT endpoint {
  caf::ipv4_address daddr;
  uint16_t dport;
};

/// Equality comparison operator for target_key.
SPOKI_CORE_EXPORT inline bool operator==(const endpoint& lhs,
                                         const endpoint& rhs) {
  return lhs.daddr == rhs.daddr && lhs.dport == rhs.dport;
}

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, endpoint& x) {
  return f.object(x).fields(f.field("daddr", x.daddr),
                            f.field("dport", x.dport));
}

} // namespace spoki::net

namespace std {

template <>
struct hash<spoki::net::endpoint> {
  size_t operator()(const spoki::net::endpoint& x) const {
    size_t seed = 0;
    spoki::hash_combine(seed, x.daddr);
    spoki::hash_combine(seed, x.dport);
    return seed;
  }
};

} // namespace std

