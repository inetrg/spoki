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

#include "perf/packet_provider.hpp"

#include "spoki/atoms.hpp"
#include "spoki/net/tcp.hpp"
#include "spoki/packet.hpp"
#include "spoki/time.hpp"

namespace perf {

caf::behavior packet_provider(caf::stateful_actor<packet_info>* self,
                              std::vector<caf::actor> router, uint32_t pps) {
  auto& s = self->state;
  // Initialize packet.
  s.pkt.ttl = s.ttl;
  s.pkt.ipid = s.ipid;
  s.pkt.daddr = caf::ipv4_address::from_bits(0x0102030a);
  s.pkt.proto = spoki::net::tcp{
    uint16_t{42342},    // sport
    uint16_t{3928},     // dport
    uint32_t{13143124}, // snum
    uint32_t{4342232},  // anum
    true,               // syn
    false,              // ack
    false,              // rst
    false,              // fin
    uint16_t{1024},     // window_size
    {},                 // options
    {},                 // payload
  };
  self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
  return {
    [=](spoki::tick_atom) {
      auto& s = self->state;
      self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
      for (uint32_t i = 0; i < pps; ++i) {
        s.daddr = ((s.daddr + 1) & s.daddr_suffix_max);
        auto complete_daddr = htonl(s.daddr | s.daddr_prefix);
        s.pkt.saddr = caf::ipv4_address::from_bits(complete_daddr);
        s.pkt.observed = spoki::make_timestamp();
        // Finish packet.
        if (router.empty()) {
          std::cerr << "no route for: " << to_string(s.pkt.saddr) << std::endl;
          return;
        }
        auto last_byte = s.pkt.saddr[3];
        auto idx = last_byte % router.size();
        auto worker = router[idx];
        self->send(worker, s.pkt);
      }
    },
  };
}

const char* packet_info::name = "packet provider";

} // namespace perf
