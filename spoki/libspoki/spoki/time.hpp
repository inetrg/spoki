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

#include <chrono>

#include <caf/timestamp.hpp>

#include "spoki/detail/core_export.hpp"

namespace spoki {

/// Stick to the timestamp type in CAF.
using timestamp = caf::timestamp;

/// Stick to the timespan type in CAF.
using timespan = caf::timespan;

/// Create new timestamps.
using caf::make_timestamp;

/// Conversion from timeval to a C++ time point.
SPOKI_CORE_EXPORT std::chrono::system_clock::time_point to_time_point(timeval tv);

/// Conversion from timeval to a C++ duration.
SPOKI_CORE_EXPORT std::chrono::system_clock::duration to_duration(timeval tv);

/// Conversion to timeval for pretty printing.
SPOKI_CORE_EXPORT timeval to_timeval(timestamp tp);

inline spoki::timestamp::rep to_count(const timestamp& ts) {
  using namespace std::chrono;
  auto ep = time_point_cast<milliseconds>(ts).time_since_epoch();
  auto in_ms = duration_cast<milliseconds>(ep);
  return in_ms.count();
}

} // namespace spoki
