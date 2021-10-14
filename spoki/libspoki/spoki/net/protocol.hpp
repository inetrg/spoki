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

#include <string>
#include <type_traits>

#include <caf/default_enum_inspect.hpp>
#include <caf/string_view.hpp>

#include "spoki/detail/core_export.hpp"

namespace spoki::net {

enum class protocol : uint8_t {
  icmp = 0,
  tcp,
  udp,
  other,
};

using protocol_utype = typename std::underlying_type<spoki::net::protocol>::type;

template <class T>
constexpr typename std::underlying_type<T>::type as_utype(const T p) {
  using utype = typename std::underlying_type<T>::type;
  return static_cast<utype>(p);
}

SPOKI_CORE_EXPORT std::string to_string(protocol);

SPOKI_CORE_EXPORT bool from_string(caf::string_view, protocol&);

SPOKI_CORE_EXPORT bool from_integer(std::underlying_type_t<protocol>,
                                    protocol&);

template <class Inspector>
bool inspect(Inspector& f, protocol& x) {
  return caf::default_enum_inspect(f, x);
}


} // namespace spoki::net
