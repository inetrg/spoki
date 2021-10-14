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

#include "spoki/net/udp_hdr.hpp"

#include <nlohmann/json.hpp>

#include "spoki/config.hpp"

// For convenience.
using json = nlohmann::json;

namespace spoki::net {

// -- JSON ---------------------------------------------------------------------

void to_json(json& j, const udp_hdr& x) {
  j = json{{"sport", x.sport},
           {"dport", x.dport},
           {"length", x.length},
           {"chksum", x.chksum}};
}

void from_json(const json&, udp_hdr&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}

} // namespace spoki::net
