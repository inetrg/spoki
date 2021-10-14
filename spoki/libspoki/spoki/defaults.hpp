/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2021
 * Authors: Raphael Hiesgen
 * All rights reserved.
 *
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#pragma once

#include "time.hpp"

namespace spoki::defaults {

// Headers for CSV files.
constexpr auto raw_csv_header = std::string_view{
  "ts|saddr|daddr|ipid|ttl|proto|sport|dport|anum|snum|options|payload|syn|"
  "ack|rst|fin|window size|probed|method|userid|probe anum|probe snum|num "
  "probes\n"};
constexpr auto scamper_csv_header = std::string_view{
  "start sec|start usec|method|userid|num probes|saddr|daddr|sport|dport\n"};
constexpr auto trace_csv_header
  = std::string_view{"ts|accepted|filtered|captured|errors|dropped|missing\n"};

namespace cache {

// Timeing to clean up our cache.
constexpr auto cache_cleanup_interval = std::chrono::minutes(5);
constexpr auto cache_cleanup_timeout = std::chrono::minutes(60);

// Chance to retain an entry instead of removing it, one in ...
constexpr auto removal_chance = int{4};
constexpr auto removal_bottom = int{1};

// Interval for port generation: https://en.wikipedia.org/wiki/Ephemeral_port
constexpr auto port_top = std::numeric_limits<uint16_t>::max();
constexpr auto port_bottom = uint16_t{49152};

// Number of probes for protocols.
constexpr auto num_tcp_probes = uint8_t{1};
constexpr auto num_tcp_rst_probes = uint8_t{2};
constexpr auto num_udp_probes = uint8_t{5};
constexpr auto num_icmp_probes = uint8_t{5};

// Reply evaluation constants, make this configureable.
constexpr auto expected_probe_replies = size_t{4};
constexpr auto reply_timeout = std::chrono::seconds{20};

// Delay before sending a TCP reset.
constexpr auto reset_delay = std::chrono::seconds{5};

} // namespace cache

} // namespace spoki::defaults
