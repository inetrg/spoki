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
#include "spoki/trace/reader.hpp"

#include "batch_reader.hpp"
#include "is_ip_address.hpp"
#include "udp_processor.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

// Some constants.
constexpr bool live_mode = true;
constexpr uint32_t batch_size = 1;

// Some CLI stuff.
class configuration : public actor_system_config {
public:
  string uri;
  string src;
  string dst;
  string addr;
  bool service_specific_probes = false;
  configuration() {
    load<io::middleman>();
    opt_group{custom_options_, "global"}
      .add(addr, "addr,a", "set our local host address")
      .add(uri, "uri,u", "set libtract uri")
      .add(src, "source,s", "set source file")
      .add(dst, "destiantion,d", "set destination file")
      .add(service_specific_probes, "service-specific-probes,S",
           "send service specific probes");
  }
};

} // namespace

void caf_main(actor_system& system, const configuration& config) {
  if (config.uri.empty()) {
    cerr << "Please specify a URI for libtract to read from with -u" << endl;
    return;
  }
  if (config.src.empty()) {
    cerr << "Please specify a file to read with -s" << endl;
    return;
  }
  if (config.addr.empty()) {
    cerr << "Please specify the local host address with -a" << endl;
    return;
  }
  caf::ipv4_address my_addr;
  auto res = caf::parse(config.addr, my_addr);
  if (res) {
    cerr << "Could not parse '" << config.addr
         << "' into an ipv4 address: " << to_string(res) << endl;
    return;
  }
  {
    scoped_actor self{system};

    // Open output file.
    ofstream out_file;
    ostream& out = config.dst.empty() ? std::cout : out_file;
    if (!config.dst.empty()) {
      out_file.open(config.dst);
      if (!out_file.is_open()) {
        aout(self) << "Failed to open file: '" << config.dst << "'"
                   << std::endl;
        return;
      }
    }

    // Start reader.
    auto source = system.spawn(batch_reader, config.src);
    if (!source) {
      aout(self) << "Could not start source with file: '" << config.src << "'"
                 << std::endl;
      return;
    }

    // Spawn prober.
    auto reflect = caf::get_or(system.config(), "probers.reflect", false);
    auto ssp = caf::get_or(system.config(), "probers.service-specific-probes",
                           false);
    auto backend = spoki::probe::udp_prober::make(ssp, reflect);
    if (!backend) {
      std::cerr << "ERR: Failed to create prober." << std::endl;
      return;
    }
    auto prober = system.spawn(spoki::probe::prober, backend);

    // Spawn processing thing.
    vector<actor> workers;
    workers.push_back(system.spawn(up::udp_processor, prober, source, my_addr));

    // Start libtrace reader.
    auto capture = system.spawn(spoki::trace::reader, workers);
    auto success = false;
    self
      ->request(capture, std::chrono::seconds(5), spoki::trace_atom_v,
                config.uri, live_mode, batch_size)
      .receive(
        [&](spoki::success_atom) {
          success = true;
          // good, let's collect some stats
        },
        [&](error&) {
          success = false;
          aout(self) << "Starting libtrace reader '" << config.uri << "' failed"
                     << endl;
        });
    if (!success) {
      self->send(prober, spoki::done_atom_v);
      self->send(capture, spoki::done_atom_v);
      for (auto& w : workers)
        self->send(w, spoki::done_atom_v);
      return;
    }

    // Now that libtrace runs, we can start sending probes.
    for (auto& w : workers)
      self->send(w, spoki::start_atom_v);

    bool running = true;
    self->receive_while(
      running)([&](const std::string& str) { out << str << std::endl; },
               [&](spoki::done_atom) {
                 aout(self) << "Done, shutting everything down." << endl;
                 running = false;
                 self->send(prober, spoki::done_atom_v);
                 self->send(source, spoki::done_atom_v);
                 for (auto& w : workers)
                   self->send(w, spoki::done_atom_v);
               });
    cout << " ### Bye! ### " << endl;
  }
  system.await_all_actors_done();
}

CAF_MAIN()
