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

#include <caf/all.hpp>

#include "perf/counting.hpp"

#include "spoki/packet.hpp"
#include "spoki/probe/request.hpp"

namespace perf {

caf::behavior counter(caf::stateful_actor<counts>* self) {
  self->state.requests = 0;
  self->delayed_send(self, std::chrono::seconds(1), spoki::stats_atom_v);
  return {
    [=](uint32_t num) { self->state.requests += num; },
    [=](const spoki::probe::request& req) { ++self->state.requests; },
    [=](const spoki::probe::request& req, bool) { ++self->state.requests; },
    [=](const spoki::packet& pkt) { ++self->state.requests; },
    [=](spoki::stats_atom) {
      auto& s = self->state;
      self->delayed_send(self, std::chrono::seconds(1), spoki::stats_atom_v);
      aout(self) << s.requests << std::endl;
      s.requests = 0;
    },
  };
}

caf::behavior aggregator(caf::stateful_actor<counts>* self, caf::actor cntr) {
  self->state.requests = 0;
  self->state.stats = 0;
  //self->delayed_send(self, std::chrono::seconds(1), spoki::stats_atom_v);
  auto add = [self, cntr]() {
    auto& s = self->state;
    s.requests += 1;
    s.stats += 1;
    if (s.requests >= 1000) {
      self->send(cntr, s.requests);
      s.requests = 0;
    }
  };
  return {
    [=](const spoki::probe::request& req) { add(); },
    [=](const spoki::probe::request& req, bool) { add(); },
    [=](const spoki::packet& pkt) { add(); },
    [=](const std::vector<spoki::packet>& packets) {
      auto& s = self->state;
      s.requests += packets.size();
      if (s.requests >= 1000) {
        self->send(cntr, s.requests);
        s.requests = 0;
      }
    },
    [=](spoki::stats_atom) {
      auto& s = self->state;
      self->delayed_send(self, std::chrono::seconds(1), spoki::stats_atom_v);
      aout(self) << "[" << self->id() << "] " << s.stats << std::endl;
      s.stats = 0;
    },
  };
}

const char* counts::name = "counter";

} // namespace perf
