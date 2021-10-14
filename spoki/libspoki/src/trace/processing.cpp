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

#include "spoki/trace/processing.hpp"
#include "spoki/trace/state.hpp"

namespace spoki::trace {

void* start_processing(libtrace_t*, libtrace_thread_t*, void* gs) {
  auto global_ptr = reinterpret_cast<global*>(gs);
  // Create and initialize our local state.
  auto c = global_ptr->make_local();
  // Increment reference, "held by libtrace".
  c->ref();
  return reinterpret_cast<void*>(c.get());
}

libtrace_packet_t* per_packet(libtrace_t*, libtrace_thread_t*, void*, void* tls,
                              libtrace_packet_t* packet) {
  auto local_ptr = reinterpret_cast<local*>(tls);
  local_ptr->add_packet(packet);
  return packet;
}

void stop_processing(libtrace_t* trace, libtrace_thread_t* thread, void*,
                     void* tls) {
  auto local_ptr = reinterpret_cast<local*>(tls);
  // Send outstanding requests.
  local_ptr->send_all();
  // Stats ...
  local_ptr->publish_stats(trace, thread);
  // Remove libtrace pointer.
  local_ptr->deref();
}

} // namespace spoki::trace
