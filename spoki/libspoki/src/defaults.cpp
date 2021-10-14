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

#include "spoki/defaults.hpp"

#include <chrono>
#include <limits>

namespace spoki {
namespace defaults {

namespace cache {

const std::chrono::minutes cache_cleanup_interval = std::chrono::minutes(5);
const std::chrono::minutes cache_cleanup_timeout = std::chrono::minutes(60);

const int removal_chance = 4;
const int removal_bottom = 1;

const uint16_t port_top = std::numeric_limits<uint16_t>::max();
const uint16_t port_bottom = 49152;

const uint8_t num_tcp_probes = 1;
const uint8_t num_tcp_rst_probes = 2;
const uint8_t num_udp_probes = 5;
const uint8_t num_icmp_probes = 5;

const size_t expected_probe_replies = 4;
const std::chrono::seconds reply_timeout = std::chrono::seconds(20);

const std::chrono::seconds reset_delay = std::chrono::seconds(5);

} // namespace cache

} // namespace defaults
} // namespace spoki
