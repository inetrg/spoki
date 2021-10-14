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
#include <cstdint>
#include <iostream>
#include <iterator>
#include <sstream>

#include <caf/all.hpp>
#include <caf/io/all.hpp>
#include <caf/string_algorithms.hpp>

#include "spoki/all.hpp"
#include "spoki/cache/shard.hpp"
#include "spoki/collector.hpp"
#include "spoki/probe/udp_prober.hpp"
#include "spoki/scamper/broker.hpp"
#include "spoki/trace/processing.hpp"
#include "spoki/trace/reader.hpp"
#include "spoki/trace/reporting.hpp"

#include "is_ip_address.hpp"
#include "perf/counting.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

class configuration : public caf::actor_system_config {
public:
  std::string uri;
  std::string dir;
  uint32_t ingest_threads = 8;
  uint32_t shards = 2;
  size_t batch_size = 1;
  bool ingest_stats = false;

  configuration() {
    opt_group{custom_options_, "global"}
      .add(uri, "uri,u", "set libtrace uri")
      .add(dir, "dir,d", "set log directory")
      .add(ingest_threads, "ingets-threads,t",
           "number of threads for packet ingestion (8)")
      .add(ingest_stats, "ingest-state,i", "print libtrace stats")
      .add(shards, "shards,s", "configure number of shards")
      .add(batch_size, "batch-size,b",
           "set batch size for forwarding incoming packets (1)");
    // nop
  }
};

void caf_main(actor_system& system, const configuration& config) {
  using addr_vec = std::vector<std::pair<std::string, uint16_t>>;

  if (config.uri.empty()) {
    std::cerr << "please specify an URI for libtrace input using '-u'.\n";
    return;
  }

  // A scope for the scoped actor.
  {
    scoped_actor self{system};
    std::vector<caf::actor> shards;
    bool experienced_failure = false;

    // -- create shards --------------------------------------------------------

    caf::actor icmp_dummy;
    caf::actor udp_dummy;

    auto accounting = system.spawn(perf::counter);
    for (uint32_t i = 0; i < config.shards; ++i)
      shards.emplace_back(system.spawn<caf::detached>(
        spoki::cache::shard,
        system.spawn<caf::detached>(perf::aggregator, accounting),
        // accounting,
        icmp_dummy, udp_dummy));

    // -- I/O ------------------------------------------------------------------

    auto tcp_collector = system.spawn<caf::detached>(
      spoki::collector, config.dir, "raw", "tcp",
      std::string{spoki::defaults::raw_csv_header}, 54321u);

    for (auto& s : shards) {
      self->send(s, spoki::collect_tcp_atom_v, spoki::start_atom_v,
                 system.spawn<caf::detached>(spoki::buffer_default,
                                             tcp_collector));
    }

    // -- create trace reader --------------------------------------------------

    auto reader = system.spawn(spoki::trace::reader, shards);

    // And send the URI for reading.
    aout(self) << "starting libtrace reader with " << config.ingest_threads
               << " threads\n";
    aout(self) << "will read from '" << config.uri << "'\n";
    self
      ->request(reader, std::chrono::seconds(5), spoki::trace_atom_v,
                config.uri, config.ingest_threads, config.batch_size)
      .receive(
        [&](spoki::success_atom) {
          // good, let's collect some stats
          // if (!config.dir.empty())
          //   self->send(reader, spoki::stats_atom_v, spoki::start_atom_v,
          //              config.dir);
        },
        [&](error& err) {
          aout(self) << "starting libtrace reader failed: " << to_string(err)
                     << endl;
          experienced_failure = true;
        });
    if (config.ingest_stats) {
      self->send(reader, spoki::stats_atom_v, spoki::start_atom_v);
      std::cout << "requesting stats\n";
    }
    std::cout << "should be up and running" << std::endl;

    // -- wait for everything to finish ----------------------------------------

    if (experienced_failure) {
      aout(self) << "experienced failure during startup\n";
      for (auto& a : shards) {
        self->send_exit(a, caf::exit_reason::user_shutdown);
      }
      self->send(reader, caf::exit_reason::user_shutdown);
    } else {
      // Only crash if all actors shut down.
      system.await_all_actors_done();
      aout(self) << "WARN: shutting down unexpectedly" << std::endl;
    }
  }
  system.await_all_actors_done();
}

} // namespace

CAF_MAIN(id_block::spoki_core, ::io::middleman)
