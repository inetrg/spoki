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
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <caf/all.hpp>

#include "spoki/config.hpp"
#include "spoki/detail/core_export.hpp"
#include "spoki/trace/instance.hpp"

SPOKI_PUSH_WARNINGS
#include <libtrace_parallel.h>
SPOKI_POP_WARNINGS

namespace spoki::trace {

/// Unqiue pointer to hold a libtrace raw pointer.
using trace_ptr = std::unique_ptr<libtrace_t, std::function<void(libtrace_t*)>>;

/// Unqiue ptr to hold a raw pointer to a libtrace callback set.
using callback_set_ptr
  = std::unique_ptr<libtrace_callback_set_t,
                    std::function<void(libtrace_callback_set_t*)>>;

/// State for a reader actor which manages libtrace instances.
struct SPOKI_CORE_EXPORT reader_state {
  uint64_t ids;
  std::unordered_map<uint64_t, std::shared_ptr<trace::global>> states;
  std::unordered_map<uint64_t, trace::instance> traces;
  std::unordered_set<caf::ipv4_address> filter;
  std::vector<caf::actor> probers;
  bool statistics;
  caf::actor stats_handler;
  static const char* name;
  // stats
  size_t dropped = 0;
  size_t accepted = 0;
  size_t errors = 0;
};

/// Returns the behavior for a reader actor which will route data
/// from incoming packets to the `probers` where data from the same
/// IP should end up at the same probe actor. The forwarded data
/// includes the IP address, the receipt timestamp and its IP ID.
SPOKI_CORE_EXPORT caf::behavior reader(caf::stateful_actor<reader_state>* self,
                                       std::vector<caf::actor> probers);

/// Returns the behavior for a reader actor which will route data
/// from incoming packets to the `probers` where data from the same
/// IP should end up at the same probe actor. The forwarded data
/// includes the IP address, the receipt timestamp and its IP ID.
SPOKI_CORE_EXPORT caf::behavior
reader_with_filter(caf::stateful_actor<reader_state>* self,
                   std::vector<caf::actor> probers,
                   std::unordered_set<caf::ipv4_address> filter);

} // namespace spoki::trace
