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

#include "spoki/trace/instance.hpp"

#include "spoki/hashing.hpp"

namespace spoki::trace {

instance::instance()
  : uri_{""},
    input_{nullptr},
    processing_{nullptr},
    reporting_{nullptr},
    global_{},
    combiner_set_{false} {
  // nop
}

instance::instance(std::string uri, trace_ptr input, callback_set processing,
                   callback_set reporting, std::shared_ptr<global> state)
  : uri_(uri),
    input_(std::move(input)),
    processing_{std::move(processing)},
    reporting_{std::move(reporting)},
    global_{std::move(state)} {
  // nop
}

instance::callback_set
instance::make_processing_callbacks(fn_cb_starting start, fn_cb_dataless stop,
                                    fn_cb_packet packet) {
  callback_set cbs{trace_create_callback_set(), trace_destroy_callback_set};
  trace_set_starting_cb(cbs.get(), start);
  trace_set_stopping_cb(cbs.get(), stop);
  trace_set_packet_cb(cbs.get(), packet);
  return cbs;
}

instance::callback_set instance::make_reporting_callbacks(fn_cb_starting start,
                                                          fn_cb_dataless stop,
                                                          fn_cb_result result) {
  callback_set cbs{trace_create_callback_set(), trace_destroy_callback_set};
  trace_set_starting_cb(cbs.get(), start);
  trace_set_stopping_cb(cbs.get(), stop);
  trace_set_result_cb(cbs.get(), result);
  return cbs;
}

caf::expected<instance> instance::create(std::string uri,
                                         callback_set processing,
                                         callback_set reporting,
                                         std::shared_ptr<global> state,
                                         uint64_t) {
  trace_ptr input{trace_create(uri.c_str()), trace_destroy};
  if (trace_is_err(input.get())) {
    std::cerr << "Could not create input" << std::endl;
    trace_perror(input.get(), "Creating trace");
    return caf::sec::runtime_error;
  }
  return instance{std::move(uri), std::move(input), std::move(processing),
                  std::move(reporting), std::move(state)};
}

void instance::set_hasher(hasher_types type, fn_hasher fun, void* data) {
  trace_set_hasher(input_.get(), type, fun, data);
  hasher_set_ = true;
}

void instance::set_combiner(const libtrace_combine_t* combiner,
                            libtrace_generic_t config) {
  trace_set_combiner(input_.get(), combiner, config);
  combiner_set_ = true;
}

void instance::set_static_hasher() {
  hasher_set_ = true;
  trace_set_hasher(input_.get(), HASHER_CUSTOM, static_hash, nullptr);
}

bool instance::start(int threads) {
  trace_set_perpkt_threads(input_.get(), threads);
  if (!combiner_set_)
    trace_set_combiner(input_.get(), &combiner_unordered,
                       libtrace_generic_t{nullptr});
  auto err = trace_pstart(input_.get(), global_.get(), processing_.get(),
                          reporting_.get());
  if (err) {
    trace_perror(input_.get(), "Starting parallel trace");
    return false;
  }
  stats_ = stats_ptr(trace_create_statistics());
  return true;
}

void instance::join() {
  trace_join(input_.get());
}

void instance::pause() {
  trace_pause(input_.get());
}

void instance::resume() {
  trace_start(input_.get());
}

void instance::update_statistics() {
  trace_get_statistics(input_.get(), stats_.get());
}

uint64_t instance::get_accepted() {
  return stats_->accepted;
}

uint64_t instance::get_filtered() {
  return stats_->filtered;
}

uint64_t instance::get_received() {
  return stats_->received;
}

uint64_t instance::get_captured() {
  return stats_->captured;
}

uint64_t instance::get_errors() {
  return stats_->errors;
}

uint64_t instance::get_dropped() {
  return stats_->dropped;
}

uint64_t instance::get_missing() {
  return stats_->missing;
}

} // namespace spoki::trace
