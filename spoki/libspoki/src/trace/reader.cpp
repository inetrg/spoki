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

#include "spoki/trace/reader.hpp"

#include "spoki/atoms.hpp"
#include "spoki/collector.hpp"
#include "spoki/logger.hpp"
#include "spoki/trace/processing.hpp"
#include "spoki/trace/reporting.hpp"
#include "spoki/trace/state.hpp"

namespace spoki::trace {

namespace {

constexpr auto trace_header = "timestamp,accepted,filtered,captured,errors,"
                              "dropped,missing";

} // namespace

caf::behavior reader(caf::stateful_actor<reader_state>* self,
                     std::vector<caf::actor> probers) {
  return reader_with_filter(self, std::move(probers), {});
}

caf::behavior reader_with_filter(caf::stateful_actor<reader_state>* self,
                                 std::vector<caf::actor> probers,
                                 std::unordered_set<caf::ipv4_address> filter) {
  self->state.ids = 0;
  self->state.statistics = false;
  self->state.filter = std::move(filter);
  self->state.probers = std::move(probers);
  return {
    [=](trace_atom, std::string uri, uint32_t threads,
        size_t batch_size) -> caf::result<success_atom> {
      auto& s = self->state;
      auto next_id = s.ids++;
      auto state = std::make_shared<trace::global>(self->system(),
                                                   self->state.probers, self,
                                                   next_id, batch_size,
                                                   self->state.filter);
      s.states[next_id] = state;
      auto p = instance::make_processing_callbacks(trace::start_processing,
                                                   trace::stop_processing,
                                                   trace::per_packet);
      auto r = instance::make_reporting_callbacks(trace::start_reporting,
                                                  trace::stop_reporting,
                                                  trace::per_result);
      auto einst = instance::create(std::move(uri), std::move(p), std::move(r),
                                    state, next_id);
      if (!einst)
        return caf::make_error(caf::sec::invalid_argument);
      if (threads == 1)
        einst->set_static_hasher();
      auto started = einst->start(threads);
      if (!started)
        return caf::make_error(caf::sec::bad_function_call);
      s.traces[next_id] = std::move(*einst);
      return success_atom_v;
    },
    [=](report_atom, uint64_t id, trace::tally t) {
      auto& s = self->state;
      // Should halve already stopped by now, just making sure.
      s.traces[id].join();
      s.traces.erase(id);
      s.states.erase(id);
      CS_LOG_DEBUG("Processed " << t.total_packets << " packets ("
                                << t.ipv4_packets << " IPv4, " << t.ipv6_packets
                                << " IPv6, " << t.others << " other)");
      // Prevent waring without logging.
      static_cast<void>(t);
      self->send(self, done_atom_v);
    },
    // Some statistics for testing the whole thing.
    [=](stats_atom, start_atom, std::string path) {
      self->send(self->state.stats_handler, done_atom_v);
      auto id = "stats-" + std::string(self->state.name) + "-"
                + std::to_string(self->id()) + "-"
                + caf::to_string(self->node());
      self->state.stats_handler
        = self->system().spawn(collector, std::move(path), std::string{"trace"},
                               std::string{"stats"}, std::string{trace_header},
                               static_cast<uint32_t>(self->id()));
      if (!self->state.statistics) {
        self->send(self, stats_atom_v);
        self->state.statistics = true;
      }
    },
    [=](stats_atom, start_atom) {
      if (!self->state.statistics) {
        self->send(self, stats_atom_v);
        self->state.statistics = true;
      }
    },
    [=](stats_atom, stop_atom) {
      self->state.statistics = false;
      self->send(self->state.stats_handler, done_atom_v);
    },
    [=](stats_atom) {
      auto& s = self->state;
      if (s.traces.empty()) {
        aout(self) << "no traces to collect statistics from\n";
        return;
      }
      if (self->state.statistics) {
        self->delayed_send(self, std::chrono::seconds(1), stats_atom_v);
      }
      auto& tr = s.traces.begin()->second;
      tr.update_statistics();
      auto acc = tr.get_accepted();
      auto err = tr.get_errors();
      auto dro = tr.get_dropped();
      std::string str = std::to_string(self->id());
      // str += "|";
      // str += std::to_string(make_timestamp().time_since_epoch().count());
      str += " | a: ";
      str += std::to_string(acc - s.accepted);
      str += ", e: ";
      str += std::to_string(err - s.errors);
      str += ", d: ";
      str += std::to_string(dro - s.dropped);
      str += "\n";
      aout(self) << str;
      s.accepted = acc;
      s.errors = err;
      s.dropped = dro;
    },
    [=](done_atom) {
      for (auto& r : self->state.traces)
        r.second.pause();
      self->quit();
    },
  };
}

const char* reader_state::name = "reader";

} // namespace spoki::trace
