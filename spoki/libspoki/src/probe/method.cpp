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

#include "spoki/probe/method.hpp"

namespace spoki::probe {

std::string probe_name(method m) {
  switch (m) {
    case method::icmp_echo:
      return "icmp-echo";
    case method::icmp_time:
      return "icmp-time";
    case method::tcp_syn:
      return "tcp-syn";
    case method::tcp_ack:
      return "tcp-ack";
    case method::tcp_ack_sport:
      return "tcp-ack-sport";
    case method::tcp_synack:
      return "tcp-synack";
    case method::tcp_rst:
      return "tcp-rst";
    case method::udp:
      return "udp";
    case method::udp_dport:
      return "udp-dport";
    default: {
      auto str = "unknown method type: " + std::to_string(static_cast<uint8_t>(m));
      return str;
    }
  }
}

} // namespace spoki::probe
