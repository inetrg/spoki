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
#include <type_traits>

#include <caf/is_error_code_enum.hpp>
#include <caf/string_view.hpp>

#include "spoki/atoms.hpp"

namespace spoki::scamper {

/// Scamper broker specific errors related to starting the broker along its
/// decoder.
enum class ec : uint8_t {
  /// The error is a lie.
  success = 0,
  /// Failed to connect to scamper daemon
  failed_to_connect = 1,
  /// Failed to start WARTS decoder.
  failed_to_start_decoder
};

SPOKI_CORE_EXPORT std::string to_string(ec);

SPOKI_CORE_EXPORT bool from_string(caf::string_view, ec&);

SPOKI_CORE_EXPORT bool from_integer(std::underlying_type_t<ec>,
                                    ec&);

// -- Serialization ------------------------------------------------------------

template <class Inspector>
bool inspect(Inspector& f, ec& x) {
  return caf::default_enum_inspect(f, x);
}

} // namespace spoki::scamper

CAF_ERROR_CODE_ENUM(spoki::scamper::ec)
