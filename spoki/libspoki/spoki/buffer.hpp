/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2019
 * Authors: Raphael Hiesgen
 *
 * All rights reserved.
 *
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#pragma once

#include <vector>

#include <caf/all.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/time.hpp"

namespace spoki {

constexpr std::size_t kB = 1024;
constexpr std::size_t MB = 1024 * kB;
constexpr std::size_t GB = 1024 * MB;

constexpr std::time_t secs_per_hour = 3600;

// constexpr auto buffer_reserve_mem = 128 * MB;
// constexpr auto buffer_send_mem = 129 * MB;
constexpr auto buffer_reserve_mem = 17 * MB;
constexpr auto buffer_send_mem = 16 * MB;

struct SPOKI_CORE_EXPORT buffer_state {
  bool got_backup_buffer = false;
  std::vector<char> buffer;
  std::vector<char> next_buffer;
  size_t write_threshold = buffer_send_mem;
  size_t reserve_size = buffer_reserve_mem;
  caf::actor collector;
  std::time_t unix_ts;

  static inline const char* name = "buffer";
};

SPOKI_CORE_EXPORT caf::behavior
buffer_default(caf::stateful_actor<buffer_state>* self, caf::actor collector);

SPOKI_CORE_EXPORT caf::behavior
buffer(caf::stateful_actor<buffer_state>* self, caf::actor collector,
       size_t reserve_size, size_t write_threshold);

} // namespace spoki
