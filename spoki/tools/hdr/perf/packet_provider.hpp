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

#include <caf/all.hpp>

#include "spoki/atoms.hpp"
#include "spoki/packet.hpp"

namespace perf {

struct packet_info {
  /// State for our packet.
  spoki::packet pkt;

  /// This actors name or debugging.
  static const char* name;

  // State for packet building.
  uint32_t daddr = 1;
  uint32_t daddr_prefix = 0x0A800000;
  uint32_t daddr_suffix_max = 0x007fffff;
  const uint8_t ttl = 202;
  const uint16_t ipid = 8247;
};

caf::behavior packet_provider(caf::stateful_actor<packet_info>* self,
                              std::vector<caf::actor> router, uint32_t pps);
} // namespace perf
