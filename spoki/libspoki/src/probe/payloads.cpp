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

#include "spoki/probe/payloads.hpp"

#include <iomanip>
#include <sstream>

namespace spoki::probe {

namespace {

struct hex {
  hex(char c) : c(static_cast<unsigned char>(c)) {
    // nop
  }
  unsigned char c;
};

inline std::ostream& operator<<(std::ostream& o, const hex& hc) {
  return (o << std::setw(2) << std::setfill('0') << std::hex
            << static_cast<int>(hc.c));
}

} // namespace

payload_map get_payloads() {
  payload_map payloads;
  // TODO: Add payloads here.
  // - Write loader for ZMap payloads available here:
  //   https://github.com/zmap/zmap/tree/main/examples/udp-probes
  return payloads;
}

payload_str_map get_payload_hex_strs() {
  payload_str_map pl_strs;
  auto payloads = get_payloads();
  for (auto& p : payloads)
    pl_strs[p.first] = to_hex_string(p.second);
  return pl_strs;
}

std::string to_hex_string(const std::vector<char>& buf) {
  std::stringstream s;
  for (auto c : buf)
    s << hex{c};
  return s.str();
}

} // namespace spoki::probe
