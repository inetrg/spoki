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

#include "spoki/net/icmp.hpp"

#include <sstream>

#include <nlohmann/json.hpp>

#include "spoki/config.hpp"

// For convenience.
using json = nlohmann::json;

namespace nlohmann {

template <typename T>
struct adl_serializer<caf::optional<T>> {
  static void to_json(json& j, const caf::optional<T>& opt) {
    if (opt)
      j = *opt;
    else
      j = nullptr;
  }

  static void from_json(const json& j, caf::optional<T>& opt) {
    if (j.is_null())
      opt = caf::none;
    else
      opt = j.get<T>();
  }
};

} // namespace

namespace spoki::net {

std::string to_string(const icmp& x) {
  std::stringstream s;
  s << "icmp(sport " << to_string(x.type) << ")";
  return s.str();
}

// -- JSON ---------------------------------------------------------------------

void to_json(json& j, const icmp& x) {
  j = json{{"type", to_string(x.type)},
           {"unreachable", x.unreachable}};
}

void from_json(const json&, icmp&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}

} // namespace spoki::net
