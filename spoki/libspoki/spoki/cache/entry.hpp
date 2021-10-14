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

#include "spoki/detail/core_export.hpp"
#include "spoki/time.hpp"

namespace spoki::cache {

/// Data cache entry containing the timestamp of the last measurement along
/// with a spoofing assumption. `true` means that the node behing the key
/// address is assumed not to be spoofed while `false` only signifies that it
/// its status is unknown.
struct SPOKI_CORE_EXPORT entry {
  timestamp ts;
  bool consistent;
};

/// Equality comparison operator for entries.
SPOKI_CORE_EXPORT inline bool operator==(const entry& lhs, const entry& rhs) {
  return lhs.ts == rhs.ts && lhs.consistent == rhs.consistent;
}

/// Inequality comparison operator for entries.
SPOKI_CORE_EXPORT inline bool operator!=(const entry& lhs, const entry& rhs) {
  return !(lhs == rhs);
}

/// Print entry using unix timestamp in ms.
SPOKI_CORE_EXPORT std::string to_string(const entry& e);

// -- Serialization ------------------------------------------------------------

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, entry& x) {
  return f.object(x).fields(f.field("ts", x.ts),
                            f.field("consistent", x.consistent));
}

} // namespace spoki::cache
