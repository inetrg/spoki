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

#include <vector>

#include <caf/actor.hpp>

#include "spoki/config.hpp"

SPOKI_PUSH_WARNINGS
#include <libtrace_parallel.h>
SPOKI_POP_WARNINGS

namespace spoki::trace {

/// Callback when the result collector starts.
void* start_reporting(libtrace_t*, libtrace_thread_t*, void*);

/// Called for each result from a processing thread.
void per_result(libtrace_t*, libtrace_thread_t*, void*, void* tls,
                libtrace_result_t* res);

/// Called after all results are processed.
void stop_reporting(libtrace_t*, libtrace_thread_t*, void*, void* tls);

} // namespace spoki::trace
