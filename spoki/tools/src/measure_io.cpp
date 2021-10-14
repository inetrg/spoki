/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2019
 * Authors: Raphael Hiesgen
 *
 * All rights reserved.
 *
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#include <chrono>
#include <iostream>

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "spoki/all.hpp"

using namespace caf;

CAF_BEGIN_TYPE_ID_BLOCK(sample, id_block::spoki_core::end)

  CAF_ADD_ATOM(sample, spoki, more_atom);

  CAF_ADD_TYPE_ID(sample, (std::vector<spoki::probe::request>) )

CAF_END_TYPE_ID_BLOCK(sample)

namespace {

// -- config -------------------------------------------------------------------

class config : public caf::actor_system_config {
public:
  bool start_server = false;
  std::string host = "localhost";
  uint16_t port = 12001;
  size_t num = 100000;
  size_t batch_size = 10;
  std::string version = "v1";

  config() {
    opt_group{custom_options_, "global"}
      .add(start_server, "server,s", "start server instead of client")
      .add(host, "host,H", "set hostname")
      .add(port, "port,P", "set port")
      .add(num, "num,n", "set message goal per second")
      .add(batch_size, "batch-size,b", "set batch size")
      .add(version, "version,v", "set version: v1, v2, v3");
  }
};

// -- logic --------------------------------------------------------------------

// v1: Just send requests as fast as we can, up to num per second.

behavior sender(event_based_actor* self, actor other, size_t num,
                spoki::probe::request req) {
  self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
  return {
    [=](spoki::tick_atom) {
      aout(self) << "tick" << std::endl;
      self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
      for (size_t i = 0; i < num; ++i) {
        self->send(other, req);
      }
    },
  };
}

struct rstate {
  size_t cnt = 0;
};

behavior receiver(stateful_actor<rstate>* self) {
  self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
  return {
    [=](spoki::probe::request& req) { self->state.cnt += req.num_probes; },
    [=](spoki::tick_atom) {
      self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
      aout(self) << self->state.cnt << std::endl;
      self->state.cnt = 0;
    },
  };
}

// v2: Let's send batches of requests.

behavior senderv2(event_based_actor* self, actor other, const size_t num,
                  const size_t batch_size, const spoki::probe::request req) {
  self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
  return {
    [=](spoki::tick_atom) {
      aout(self) << "tick" << std::endl;
      self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
      std::vector<spoki::probe::request> batch;
      batch.reserve(batch_size);
      for (size_t i = 0; i < num; ++i) {
        batch.emplace_back(req);
        if (batch.size() > batch_size) {
          self->send(other, batch);
          batch.clear();
        }
      }
    },
  };
}

behavior receiverv2(stateful_actor<rstate>* self) {
  self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
  return {
    [=](const std::vector<spoki::probe::request>& requests) {
      for (auto& req : requests)
        self->state.cnt += req.num_probes;
    },
    [=](spoki::tick_atom) {
      self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
      aout(self) << self->state.cnt << std::endl;
      self->state.cnt = 0;
    },
  };
}

// v3: Ping pong protocol where the receiver always asks for more.

behavior senderv3(event_based_actor* self, actor other, const size_t num,
                  const size_t batch_size, const spoki::probe::request req) {
  self->send(self, spoki::more_atom_v);
  return {
    [=](spoki::more_atom) {
      self->send(other, req);
    }
  };
}

behavior receiverv3(stateful_actor<rstate>* self) {
  self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
  return {
    [=](spoki::probe::request& req) {
      self->state.cnt += req.num_probes;
      return spoki::more_atom_v;
    },
    [=](spoki::tick_atom) {
      self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
      aout(self) << self->state.cnt << std::endl;
      self->state.cnt = 0;
    },
  };
}

// -- start everything ---------------------------------------------------------

void caf_main(actor_system& system, const config& config) {
  if (config.start_server) {
    actor srv;
    if (config.version == "v1") {
      srv = system.spawn(receiver);
    } else if (config.version == "v2") {
      srv = system.spawn(receiverv2);
    } else if (config.version == "v3") {
      srv = system.spawn(receiverv3);
    } else {
      std::cerr << "unsupported version\n";
      return;
    }
    system.middleman().publish(srv, config.port);
    system.await_all_actors_done();
  } else {
    auto srv = system.middleman().remote_actor(config.host, config.port);
    if (!srv) {
      std::cerr << "could not reach server: " << to_string(srv) << std::endl;
      return;
    } else {
      spoki::probe::request req;
      req.probe_method = spoki::probe::method::tcp_synack;
      req.saddr = caf::ipv4_address::from_bits(0x01020308);
      req.daddr = caf::ipv4_address::from_bits(0x00feffff);
      req.sport = 1337;
      req.dport = 80;
      req.anum = 123881;
      req.num_probes = 1; // I'll use this for couning.
      req.user_id = 8768768;
      if (config.version == "v1") {
        system.spawn(sender, *srv, config.num, req);
      } else if (config.version == "v2") {
        system.spawn(senderv2, *srv, config.num, config.batch_size, req);
      } else if (config.version == "v3") {
        system.spawn(senderv3, *srv, config.num, config.batch_size, req);
      } else {
        std::cerr << "unsupported version\n";
        return;
      }
      system.await_all_actors_done();
    }
  }
}

} // namespace

CAF_MAIN(id_block::spoki_core, id_block::sample, caf::io::middleman)
