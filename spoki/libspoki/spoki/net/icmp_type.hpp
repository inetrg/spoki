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
#include <caf/meta/type_name.hpp>
#include <caf/string_view.hpp>

#include "spoki/detail/core_export.hpp"

namespace spoki::net {

enum class icmp_type : uint8_t {
  echo_reply = 0,
  // 1 & 2 unassigned
  dest_unreachable = 3,
  source_quench,
  redirect_message,
  // 6 deprecated
  // 7 unassigned
  echo_request = 8,
  router_advertisement,
  router_solicitation,
  time_exceeded,
  bad_ip_header,
  timestamp,
  timestamp_reply,
  information_request,
  information_reply,
  addr_mark_request,
  addr_mark_reply,
  // 19 to 29 reserved
  // 30: traceroute, deprecated
  // 31 to 39 deprecated
  // 40 ...
  // 41 experimental
  extended_echo_request = 42,
  extended_echo_reply,
  // 44 to 252 reserved
  // 253 & 254 experimental
  other = 255, // 255 reserved
};

SPOKI_CORE_EXPORT std::string to_string(icmp_type);

SPOKI_CORE_EXPORT bool from_string(caf::string_view, icmp_type&);

SPOKI_CORE_EXPORT bool from_integer(std::underlying_type_t<icmp_type>,
                                    icmp_type&);

icmp_type to_icmp_type(uint8_t val);

// -- Serialization ------------------------------------------------------------

template <class Inspector>
bool inspect(Inspector& f, icmp_type& x) {
  return caf::default_enum_inspect(f, x);
}

} // namespace spoki::net
