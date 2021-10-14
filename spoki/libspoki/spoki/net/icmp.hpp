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

#include <caf/meta/type_name.hpp>
#include <caf/optional.hpp>

#include <nlohmann/json_fwd.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"
#include "spoki/net/icmp_type.hpp"
#include "spoki/net/udp_hdr.hpp"

namespace spoki::net {

/// ICMP event info.
struct SPOKI_CORE_EXPORT icmp {
  icmp_type type;
  caf::optional<udp_hdr> unreachable;
};

/// Print data of a icmp event.
SPOKI_CORE_EXPORT std::string to_string(const icmp& x);

/// Equality comparison operator for udp probes.
inline bool operator==(const icmp& lhs, const icmp& rhs) {
  return lhs.type == rhs.type;
}

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, icmp& x) {
  return f.object(x).fields(f.field("type", x.type));
}

// -- JSON ---------------------------------------------------------------------

void to_json(nlohmann::json& j, const icmp& x);

void from_json(const nlohmann::json&, icmp&);

} // namespace spoki::net

namespace std {

template <>
struct hash<spoki::net::icmp> {
  size_t operator()(const spoki::net::icmp& x) const noexcept {
    using utype = std::underlying_type<spoki::net::icmp_type>::type;
    std::hash<utype> h;
    return h(static_cast<utype>(x.type));
  }
};

} // namespace std
