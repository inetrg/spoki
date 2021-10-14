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

#pragma once

#include <caf/stateful_actor.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/scamper/driver.hpp"

namespace spoki::scamper {

// The scamper::broker is not fast enough to drive enough scamper daemons to
// give us up to 500k pps, it doesn't even reach 100k pps. Instead of using a
// single I/O loop, we'll strive to deploy mulitple ones. Each shard will get a
// sinlge scamper::manger that it sends it's request to. The manager will make
// sure that it doesn't request probes to target it aleady probes.

struct SPOKI_CORE_EXPORT mgnt_data {
  /// Driver instances for probing. Could be a map from ID to driver_ptr if we
  /// want to have more than one. In that case we should probably bundle a bunch
  /// of information with the ptr. That might include pps, the targets map, etc.
  driver_ptr drv;

  /// PPS data to check if we can keep this up. We could eventually enable
  /// dynamic allocation of probes. Well, the scamper instances have to be
  /// running, but we could dynamically distribute them across shards. We don't
  /// know how the current load will be distributed across them in advance.
  uint32_t pps;

  /// Count request per second.
  uint32_t rps;

  /// Last queue size info we got from the driver.
  size_t queue_size;

  /// Use this as a PPS goal for now.
  uint32_t pps_goal;

  /// Current targets. If we target is in this set, we should not request an
  /// additional probe.
  std::unordered_set<target_key> targets;
  std::unordered_map<uint32_t, target_key> userids;

  /// Forward scamper replies here for logging. Additionally log probes we drop.
  caf::actor collector;

  // If we want to dynamically distribute scamper instances we'd need an
  // "exchange point" for the resources. One prerequisit is that we can shut
  // down drivers, which is currently not implemented.
  //
  // caf::actor scamper_info_exchange_point.

  /// Tag for result files.
  std::string tag;

  /// Our self pointer.
  caf::stateful_actor<mgnt_data>* self;

  /// Name for CAF-related error and logs.
  const char* name;
};

SPOKI_CORE_EXPORT caf::behavior manager(caf::stateful_actor<mgnt_data>* self,
                                        std::string tag, std::string host,
                                        uint16_t port);

SPOKI_CORE_EXPORT caf::behavior
manager_unix(caf::stateful_actor<mgnt_data>* self, std::string tag,
             std::string name);

} // namespace spoki::scamper
