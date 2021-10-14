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

#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>

#include <caf/default_enum_inspect.hpp>
#include <caf/optional.hpp>
#include <caf/string_view.hpp>

#include <nlohmann/json_fwd.hpp>

#include "spoki/detail/core_export.hpp"

namespace spoki::net {

/// Socket operation to control the async decoder event subscriptions.
enum class tcp_opt : uint8_t {
  end_of_list = 0,
  noop = 1,
  mss = 2,
  window_scale = 3,
  sack_permitted = 4,
  sack = 5,
  timestamp = 8,
  other
};

using tcp_opt_map = std::unordered_map<tcp_opt, caf::optional<uint32_t>>;

SPOKI_CORE_EXPORT std::string to_string(tcp_opt);

SPOKI_CORE_EXPORT bool from_string(caf::string_view, tcp_opt&);

SPOKI_CORE_EXPORT bool from_integer(std::underlying_type_t<tcp_opt>, tcp_opt&);

std::string option_name(tcp_opt);

inline constexpr uint8_t to_value(tcp_opt x) {
  return static_cast<uint8_t>(x);
}

// -- JSON ---------------------------------------------------------------------

void to_json(nlohmann::json& j, const tcp_opt_map& x);

void from_json(const nlohmann::json&, tcp_opt_map&);

// -- Serialization ------------------------------------------------------------

template <class Inspector>
bool inspect(Inspector& f, tcp_opt& x) {
  return caf::default_enum_inspect(f, x);
}

} // namespace spoki::net

// -- HASH ---------------------------------------------------------------------

namespace std {

template <>
struct hash<spoki::net::tcp_opt> {
  size_t operator()(const spoki::net::tcp_opt x) const noexcept {
    std::hash<uint8_t> h;
    return h(to_value(x));
  }
};

} // namespace std
