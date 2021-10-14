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

#include <iostream>

#include "spoki/atoms.hpp"
#include "spoki/logger.hpp"

#include "spoki/trace/processing.hpp"
#include "spoki/trace/reporting.hpp"
#include "spoki/trace/state.hpp"

namespace spoki::trace {

void* start_reporting(libtrace_t*, libtrace_thread_t*, void*) {
  // Create the state for processing. Be sure to delete it!
  return new tally;
}

void per_result(libtrace_t*, libtrace_thread_t*, void*, void* tls,
                libtrace_result_t* res) {
  // We only want to handle results containing our user-defined
  // structure. Check result->type to see what type of result this is.
  if (res->type != RESULT_USER)
    return;
  // The key is the same as the key value that was passed into
  // trace_publish_result() when the processing thread published this
  // result. For now, our key will always be zero.
  auto key = res->key;
  // result->value is a libtrace_generic_t that was passed into
  // trace_publish_result() when the processing thread published this
  // result. In our case, we know that it is a pointer to a struct
  // result so we can cast it to the right type.
  auto r = reinterpret_cast<result*>(res->value.ptr);
  // Grab our tally out of thread local storage and update it based on
  // this new result.
  auto local_ptr = reinterpret_cast<tally*>(tls);
  local_ptr->total_packets += r->total_packets;
  local_ptr->ipv4_packets += r->ipv4_packets;
  local_ptr->ipv6_packets += r->ipv6_packets;
  local_ptr->others += r->others;
  local_ptr->last_key = key;
  // Remember that the results was malloced by the processing thread,
  // so make sure to free it here.
  delete r;
}

void stop_reporting(libtrace_t*, libtrace_thread_t*, void* gs, void* tls) {
  auto global_ptr = reinterpret_cast<global*>(gs);
  auto local_ptr = reinterpret_cast<tally*>(tls);
  // TODO: Just send the stats to the collector.
  anon_send(global_ptr->parent, spoki::report_atom_v, global_ptr->id,
            *local_ptr);
  delete local_ptr;
}

} // namespace spoki::trace
