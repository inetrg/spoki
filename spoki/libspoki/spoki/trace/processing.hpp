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

#include <caf/all.hpp>

#include "spoki/config.hpp"

SPOKI_PUSH_WARNINGS
#include <libtrace_parallel.h>
SPOKI_POP_WARNINGS

namespace spoki::trace {

/// Processing function for libtrace that creates state for each thread.
void* start_processing(libtrace_t*, libtrace_thread_t*, void*);

/// Processing function for libtrace that is called for each packet.
/// The global storage `gs` is shared across threads, while the thread
/// local storage `tls` is unique for each thread.
libtrace_packet_t* per_packet(libtrace_t*, libtrace_thread_t*, void*, void* tls,
                              libtrace_packet_t* packet);

/// Processing function called when a thread processed all its
/// packets. Results are passed to the reporting thread via
/// the publish call.
void stop_processing(libtrace_t* trace, libtrace_thread_t* thread, void*,
                     void* tls);

} // namespace spoki::trace
