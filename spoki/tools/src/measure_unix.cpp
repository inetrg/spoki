#include <chrono>
#include <iostream>

#include <arpa/inet.h>

#include "spoki/all.hpp"
#include "spoki/probe/request.hpp"
#include "spoki/scamper/driver.hpp"
#include "spoki/scamper/manager.hpp"

// -- config -------------------------------------------------------------------

class config : public caf::actor_system_config {
public:
  std::string name = "/tmp/scmp001";
  bool with_manager = false;
  size_t pps = 10000;

  config() {
    opt_group{custom_options_, "global"}
      .add(name, "name,n", "set unix domain socket name")
      .add(with_manager, "with-manger,m", "start with manager")
      .add(pps, "pps,p", "set probing rate (probes per second)");
  }
};

struct producer_state {
  uint32_t daddr_prefix = 0x0A800000;
  //uint32_t daddr_prefix = 0x08000000;
  uint32_t daddr_suffix_max = 0x007fffff;
  //uint32_t daddr_suffix_max = 0x00ffffff;
  uint32_t daddr = 1;
  uint32_t user_id_counter = 0;
  spoki::probe::request req;
};

caf::behavior producer(caf::stateful_actor<producer_state>* self,
                       caf::actor consumer, size_t num) {
  self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
  auto& s = self->state;
  s.user_id_counter = 0;
  s.req.probe_method = spoki::probe::method::tcp_synack;
  //s.req.saddr = caf::ipv4_address::from_bits(0x01020308);
  s.req.saddr = caf::ipv4_address::from_bits(0x0102030a);
  s.req.sport = 1337;
  s.req.dport = 80;
  s.req.anum = 123881;
  s.req.num_probes = 1;
  return {
    [=](spoki::tick_atom) {
      // aout(self) << "tick" << std::endl;
      self->delayed_send(self, std::chrono::seconds(1), spoki::tick_atom_v);
      auto& s = self->state;
      for (size_t i = 0; i < num; ++i) {
        // TODO: update request.
        s.req.user_id = ++s.user_id_counter;
        s.daddr = ((s.daddr + 1) & s.daddr_suffix_max);
        auto complete_daddr = htonl(s.daddr | s.daddr_prefix);
        s.req.daddr = caf::ipv4_address::from_bits(complete_daddr);
        //aout(self) << "Request: " << spoki::probe::make_command(s.req);
        self->send(consumer, s.req, true);
      }
    },
  };
}

void caf_main(caf::actor_system& system, const config& cfg) {
  if (cfg.with_manager) {
    auto mgr = system.spawn(spoki::scamper::manager_unix, "testing", cfg.name);
    system.spawn(producer, mgr, cfg.pps);
    system.await_all_actors_done();
  } else {
    // This requires the drver to generate probe request itself. That code is no
    // longer there, but easily added.
    std::cerr << "CURRENTLY NOT SUPPORTED (use -m)\n";
    return;
    auto drv = spoki::scamper::driver::make(system, cfg.name);
    drv->join();
  }
}

CAF_MAIN(caf::id_block::spoki_core, caf::io::middleman)
