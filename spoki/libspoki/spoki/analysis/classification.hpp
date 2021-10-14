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

#include <type_traits>

#include <caf/string_view.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/task.hpp"

namespace spoki::analysis {

/// Tag to classify a sequence of IP ids.
enum class classification : uint8_t {
  /// Not enough data to make a decision.
  unchecked,
  /// All ids are the same or match the ids of the probes.
  constant,
  /// The ids appear to be assigned at random.
  random,
  /// Ids in the sequence increase monotonically.
  monotonic,
  /// None of the above.
  other
};

// -- inspect requirements -----------------------------------------------------

SPOKI_CORE_EXPORT std::string to_string(classification);

SPOKI_CORE_EXPORT bool from_string(caf::string_view, classification&);

SPOKI_CORE_EXPORT bool from_integer(std::underlying_type_t<classification>,
                                    classification&);

// -- Serialization ------------------------------------------------------------

template <class Inspector>
bool inspect(Inspector& f, classification& x) {
  return caf::default_enum_inspect(f, x);
}

} // namespace spoki::analysis
