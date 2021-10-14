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

namespace spoki::probe {

/// Probing methods supported by scamper.
enum class method : uint8_t {
  icmp_echo = 0,
  icmp_time,
  tcp_syn,
  tcp_ack,
  tcp_ack_sport,
  tcp_synack,
  tcp_rst,
  udp,
  udp_dport
};

SPOKI_CORE_EXPORT std::string probe_name(method);

SPOKI_CORE_EXPORT std::string to_string(method);

SPOKI_CORE_EXPORT bool from_string(caf::string_view, method&);

SPOKI_CORE_EXPORT bool from_integer(std::underlying_type_t<method>,
                                    method&);

// -- Serialization ------------------------------------------------------------

template <class Inspector>
bool inspect(Inspector& f, method& x) {
  return caf::default_enum_inspect(f, x);
}

} // namespace spoki::probe
