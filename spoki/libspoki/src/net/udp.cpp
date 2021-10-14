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

#include "spoki/net/udp.hpp"

#include <sstream>

#include <nlohmann/json.hpp>

#include "spoki/config.hpp"
#include "spoki/probe/payloads.hpp"

// For convenience.
using json = nlohmann::json;

namespace spoki::net {

std::string to_string(const udp& x) {
  std::stringstream s;
  s << "udp(sport " << std::to_string(x.sport) << ", dport "
    << std::to_string(x.dport) << ")";
  return s.str();
}

// -- JSON ---------------------------------------------------------------------

void to_json(json& j, const udp& x) {
  j = json{{"sport", x.sport},
           {"dport", x.dport},
           {"payload", probe::to_hex_string(x.payload)}};
}

void from_json(const json&, udp&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}

} // namespace spoki::net
