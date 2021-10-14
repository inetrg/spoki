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

#include <chrono>

#include <caf/all.hpp>

#include "spoki/atoms.hpp"
#include "spoki/scamper/manager.hpp"
#include "spoki/scamper/reply.hpp"
#include "spoki/probe/request.hpp"

namespace spoki::scamper {

namespace {

caf::behavior manager_with_scamper(caf::stateful_actor<mgnt_data>* self,
                                   std::string tag) {
  self->state.drv->set_collector(self);
  // Set pps goal.
  self->state.pps_goal = 20000;
  // Send self a tick for stats.
  self->delayed_send(self, std::chrono::seconds(1), tick_atom_v);
  // Initialize state.
  self->state.tag = tag;
  self->state.self = self;
  self->state.pps = 0;
  self->state.rps = 0;
  return {
    [=](const probe::request& req, bool is_irregular) {
      // aout(self) << "got new request to " << to_string(req.daddr) << "\n";
      auto& s = self->state;
      // Check pending probes.
      auto key = target_key{req.daddr, is_irregular};
      if (s.targets.count(key) == 0) {
        // If not in targets, send to next driver.
        // aout(self) << "sending request to driver\n";
        s.drv->probe(req);
        s.rps += 1;
        // Add to targest
        s.targets.insert(key);
        s.userids[req.user_id] = key;
      } else {
        // TODO: Log that we won't probe the target?
      }
    },
    [=](const probe::request& req) {
      auto& s = self->state;
      // Check pending probes.
      auto key = target_key{req.daddr, false};
      if (s.targets.count(key) == 0) {
        // If not in targets, send to next driver.
        s.drv->probe(req);
        s.rps += 1;
        // Add to targest
        s.targets.insert(key);
        s.userids[req.user_id] = key;
      } else {
        // TODO: Log that we won't probe the target?
      }
    },
    [=](uint32_t sent_probes, size_t queue_size) {
      self->state.pps += sent_probes;
      self->state.queue_size = queue_size;
    },
    [=](probed_atom, const reply& rep) {
      auto& s = self->state;
      auto key = s.userids[rep.userid];
      s.targets.erase(key);
      s.userids.erase(rep.userid);
      self->send(s.collector, rep);
      //aout(self) << "got scamper reply\n";
      // TODO: Log these somewhere.
    },
    // Configure collector for scamper replies.
    [=](spoki::collect_atom, spoki::start_atom, caf::actor handler) {
      self->send(self->state.collector, done_atom_v);
      self->state.collector = handler;
    },
    // Stop collecting scamper replies.
    [=](collect_atom, stop_atom) {
      self->send(self->state.collector, done_atom_v);
    },
    [=](tick_atom) {
      self->delayed_send(self, std::chrono::seconds(1), tick_atom_v);
      aout(self) << "[" << self->id() << "] rps: " << self->state.rps
                 << ", pps: " << self->state.pps
                 << ", queue size: " << self->state.queue_size << std::endl;
      self->state.pps = 0;
      self->state.rps = 0;
    },
  };
}

} // anonymous

caf::behavior manager(caf::stateful_actor<mgnt_data>* self, std::string tag,
                      std::string host, uint16_t port) {
  // Create driver.
  auto drv = scamper::driver::make(self->system(), host, port);
  if (!drv) {
    aout(self) << "failed to create driver for " << host << ":" << port
               << std::endl;
    return {};
  }
  self->state.drv = drv;
  return manager_with_scamper(self, tag);
}

caf::behavior manager_unix(caf::stateful_actor<mgnt_data>* self,
                           std::string tag, std::string name) {
  // Create driver.
  auto drv = scamper::driver::make(self->system(), name);
  if (!drv) {
    aout(self) << "failed to create driver for " << name << std::endl;
    return {};
  }
  self->state.drv = drv;
  return manager_with_scamper(self, tag);
}

} // namespace spoki::scamper
