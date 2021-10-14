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
#include <string>

#include <caf/meta/type_name.hpp>

#include <nlohmann/json_fwd.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"

namespace spoki::net {

/// UDP event info.
struct SPOKI_CORE_EXPORT udp {
  uint16_t sport;
  uint16_t dport;
  std::vector<char> payload;
};

/// Equality comparison operator for udp events.
inline bool operator==(const udp& lhs, const udp& rhs) {
  return lhs.sport == rhs.sport && lhs.dport == rhs.dport;
}

/// Print data of a udp event.
SPOKI_CORE_EXPORT std::string to_string(const udp& x);

// -- Serialization ------------------------------------------------------------

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, udp& x) {
  return f.object(x).fields(f.field("sport", x.sport),
                            f.field("dport", x.dport),
                            f.field("payload", x.payload));
}

// -- JSON ---------------------------------------------------------------------

void to_json(nlohmann::json& j, const udp& x);

void from_json(const nlohmann::json&, udp&);

} // namespace spoki::net

namespace std {

template <>
struct hash<spoki::net::udp> {
  size_t operator()(const spoki::net::udp& x) const noexcept {
    size_t seed = 0;
    spoki::hash_combine(seed, x.sport);
    spoki::hash_combine(seed, x.dport);
    return seed;
  }
};

} // namespace std
