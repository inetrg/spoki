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

#include "spoki/net/tcp_opt.hpp"

#include <caf/string_view.hpp>

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

} // namespace nlohmann

namespace spoki::net {

// -- pretty string ------------------------------------------------------------

std::string option_name(tcp_opt x) {
  switch (x) {
    default:
      return "???";
    case tcp_opt::end_of_list:
      return "end_of_list";
    case tcp_opt::noop:
      return "noop";
    case tcp_opt::mss:
      return "mss";
    case tcp_opt::window_scale:
      return "window_scale";
    case tcp_opt::sack_permitted:
      return "sack_permitted";
    case tcp_opt::sack:
      return "sack";
    case tcp_opt::timestamp:
      return "timestamp";
    case tcp_opt::other:
      return "other";
  };
}

// -- JSON ---------------------------------------------------------------------

void to_json(json& j, const tcp_opt& x) {
  j = to_string(x);
}

void from_json(const json&, tcp_opt&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}

void to_json(json& j, const tcp_opt_map& x) {
  for (auto& p : x)
    j[to_string(p.first)] = p.second;
}

void from_json(const json&, tcp_opt_map&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}

} // namespace spoki::net
