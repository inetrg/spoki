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
#include <sstream>

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "spoki/probe/udp_prober.hpp"

#include "is_ip_address.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

// Some CLI stuff.
class configuration : public actor_system_config {
public:
  string host;
  uint16_t port;
  bool service_specific_probes = false;
  bool reflect = false;
  configuration() {
    load<io::middleman>();
    opt_group{custom_options_, "global"}
      .add(host, "host,H", "host to accept connections from")
      .add(port, "port,p", "port to publish on")
      .add(service_specific_probes, "service-specific-probes,S",
           "load service-specific probes map to send to matching ports")
      .add(reflect, "reflect,r",
           "reflect payload for probes not in out list "
           " services map.");
  }
};

} // namespace

void caf_main(actor_system& system, const configuration& config) {
  auto reflect = caf::get_or(system.config(), "probers.reflect", false);
  auto ssp = caf::get_or(system.config(), "probers.service-specific-probes",
                         false);
  auto backend = spoki::probe::udp_prober::make(ssp, reflect);
  if (!backend) {
    std::cerr << "ERR: Failed to create prober." << std::endl;
    return;
  }
  auto prober = system.spawn(spoki::probe::prober, backend);
  if (!prober)
    return;
  const char* host = config.host.empty() ? nullptr : config.host.c_str();
  auto port = system.middleman().publish(prober, config.port, host, true);
  if (!port) {
    cerr << "Failed to publish prober: " << to_string(port.error()) << endl;
    caf::anon_send_exit(prober, caf::exit_reason::normal);
  } else {
    cout << "Prober running on port " << *port << "." << endl;
  }
  system.await_all_actors_done();
  cout << "Bye!" << endl;
}

CAF_MAIN(caf::io::middleman)
