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

#include "spoki/cache/timeout.hpp"

#include "spoki/net/icmp.hpp"
#include "spoki/net/tcp.hpp"
#include "spoki/net/udp.hpp"

namespace spoki::cache {

namespace {

struct to_factory_visitor {
  timeout operator()(const net::icmp&) {
    return icmp_timeout{addr};
  }
  timeout operator()(const net::tcp&) {
    return tcp_timeout{addr};
  }
  timeout operator()(const net::udp&) {
    return udp_timeout{addr};
  }
  caf::ipv4_address addr;
};

} // namespace

timeout make_timeout(const packet& pkt) {
  return caf::visit(to_factory_visitor{pkt.saddr}, pkt.proto);
}

} // namespace spoki::cache
