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

#include "spoki/net/five_tuple.hpp"

#include <nlohmann/json.hpp>

#include "spoki/config.hpp"

// For convenience.
using json = nlohmann::json;

namespace spoki::net {

void to_json(json& j, const five_tuple& x) {
  j = json{{"protocol", to_string(x.proto)},
           {"saddr", to_string(x.saddr)},
           {"daddr", to_string(x.daddr)},
           {"sport", x.sport},
           {"dport", x.dport},
           };
}

void from_json(const json&, five_tuple&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}

} // namespace caf::net
