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

#include "spoki/scamper/ping.hpp"

#include <array>

namespace spoki::scamper {

std::string ping_dst(scamper_ping_t* ptr) {
  std::array<char, 16> buf = {};
  scamper_addr_tostr(ptr->dst, buf.data(), buf.size());
  return std::string(buf.data());
}

std::string ping_src(scamper_ping_t* ptr) {
  std::array<char, 16> buf = {};
  scamper_addr_tostr(ptr->src, buf.data(), buf.size());
  return std::string(buf.data());
}

} // namespace spoki::scamper
