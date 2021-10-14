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
#include <string>

#include <caf/error.hpp>
#include <caf/expected.hpp>
#include <caf/sec.hpp>

#include "spoki/config.hpp"
#include "spoki/trace/state.hpp"
#include "spoki/unique_c_ptr.hpp"
#include "spoki/detail/core_export.hpp"

SPOKI_PUSH_WARNINGS
#include <libtrace_parallel.h>
SPOKI_POP_WARNINGS

namespace spoki::trace {

/// An instance of a libtrace data stream. An not that elegant wrapper around
/// the C API. Needs some thought and rework.
class SPOKI_CORE_EXPORT instance {
  // -- member types -----------------------------------------------------------

  using trace_delete_sig = std::function<void(libtrace_t*)>;
  using cb_delete_sig = std::function<void(libtrace_callback_set_t*)>;
  using stats_ptr = unique_c_ptr<libtrace_stat_t>;

public:
  using trace_ptr = std::unique_ptr<libtrace_t, trace_delete_sig>;
  using callback_set = std::unique_ptr<libtrace_callback_set_t, cb_delete_sig>;

  // -- constructors, assignment operators, and factory ------------------------

  instance();
  instance(instance&&) = default;
  instance(const instance&) = delete;

  instance& operator=(instance&&) = default;
  instance& operator=(const instance&) = delete;

  /// Create a new libtrace instance to process data from `uri` using the
  /// `processing` callback set to process packets before combining results with
  /// with `reporting` callback set. The `state` is available to all threads
  /// at runtime (access is not thread safe). `id` identifies this instance.
  static caf::expected<instance>
  create(std::string uri, callback_set processing, callback_set reporting,
         std::shared_ptr<global> state, uint64_t id);

  // -- libtrace configuration -------------------------------------------------

  /// Creates a callback set for processing packets. `start` can return a
  /// pointer to thread-local state which can later be cleaned up in `stop`.
  /// `stop` further publish a pointer to processing results which will be
  /// processed by the reporting callbacks.`packet` will be called once for each
  /// packet to process and can return the packet pointer to have its memory
  /// managed by libtrace. See libtrace documentation for details.
  static callback_set make_processing_callbacks(fn_cb_starting start,
                                                fn_cb_dataless stop,
                                                fn_cb_packet packet);

  /// Creates a set of callbacks to aggregate the results published by
  /// processing threads. Follows a similar model to the processing
  /// callbacks.
  static callback_set make_reporting_callbacks(fn_cb_starting start,
                                               fn_cb_dataless stop,
                                               fn_cb_result result);

  /// The hasher determines which processing thread will handle a packet. This
  /// assigns a static hasher which will assign all packets to a single
  /// processing thread.
  void set_static_hasher();

  /// Configure a custom hasher function which will be used to distribute
  /// incoming packets among processing threads.
  void set_hasher(hasher_types type, fn_hasher fun = nullptr,
                  void* data = nullptr);

  /// Configure a combiner which determines how processing results from multiple
  /// threads are combined.
  void set_combiner(const libtrace_combine_t* combiner,
                    libtrace_generic_t config);

  // -- libtrace control -------------------------------------------------------

  /// Start the libtrace using `threads` for processing.
  bool start(int threads);

  /// Wait from all libtrace threads to finish processing.
  void join();

  /// Pause libtrace processing.
  void pause();

  /// Resume libtrace processing.
  void resume();

  /// Get the current processing URI.
  inline const std::string& uri() const {
    return uri_;
  }

  // -- libtrace statistics ----------------------------------------------------

  /// Update the available statistics. All calls to the statistics getters below
  /// will return the state acquired by this call.
  void update_statistics();

  /// Get statistics for the number of accepted packets.
  uint64_t get_accepted();

  /// Get statistics for the number of filtered packets.
  uint64_t get_filtered();

  /// Get statistics for the number of received packets.
  uint64_t get_received();

  /// Get statistics for the number of captured packets.
  uint64_t get_captured();

  /// Get statistics for the number of errors.
  uint64_t get_errors();

  /// Get statistics for the number of dropped packets.
  uint64_t get_dropped();

  /// Get statistics for the number of missing packets.
  uint64_t get_missing();

private:
  // -- factory constructor ----------------------------------------------------

  instance(std::string uri, trace_ptr input, callback_set processing,
           callback_set reporting, std::shared_ptr<global> state);

  // -- members ----------------------------------------------------------------

  std::string uri_;
  trace_ptr input_;
  callback_set processing_;
  callback_set reporting_;
  std::shared_ptr<global> global_;
  bool hasher_set_;
  bool combiner_set_;
  stats_ptr stats_;
};

} // namespace spoki::trace
