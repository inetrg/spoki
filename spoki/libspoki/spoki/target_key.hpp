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

#include "caf/ipv4_address.hpp"

#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"

namespace spoki {

/// Info to determin if we should request a probe to a target. Protocol is
/// omitted because we use separate sets to check.
struct SPOKI_CORE_EXPORT target_key {
  caf::ipv4_address saddr;
  bool is_scanner_like;
};

/// Equality comparison operator for target_key.
SPOKI_CORE_EXPORT inline bool operator==(const target_key& lhs,
                                         const target_key& rhs) {
  return lhs.saddr == rhs.saddr && lhs.is_scanner_like == rhs.is_scanner_like;
}

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, target_key& x) {
  return f.object(x).fields(f.field("saddr", x.saddr),
                            f.field("is_scanner_like", x.is_scanner_like));
}

} // namespace spoki

namespace std {

template <>
struct hash<spoki::target_key> {
  size_t operator()(const spoki::target_key& x) const {
    size_t seed = 0;
    spoki::hash_combine(seed, x.saddr);
    spoki::hash_combine(seed, x.is_scanner_like);
    return seed;
  }
};

} // namespace std
