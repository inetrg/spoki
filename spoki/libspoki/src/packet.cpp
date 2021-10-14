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

#include "spoki/packet.hpp"

#include <sstream>

#include <nlohmann/json.hpp>

#include "spoki/config.hpp"
#include "spoki/time.hpp"

// For convenience.
using json = nlohmann::json;

namespace {

struct vis_jsonify_pkt {
  void operator()(const spoki::net::icmp& x) const {
    j = x;
  }
  void operator()(const spoki::net::tcp& x) const {
    j = x;
  }
  void operator()(const spoki::net::udp& x) const {
    j = x;
  }
  json& j;
};

} // namespace

namespace nlohmann {

template <typename... Args>
struct adl_serializer<caf::variant<Args...>> {
  static void to_json(json& j, caf::variant<Args...> const& v) {
    caf::visit(vis_jsonify_pkt{j}, v);
  }
};



} // namespace nlohmann

namespace spoki {

// -- visitor to dispatch protocol ---------------------------------------------

struct vis_key_builder {
  target_key operator()(const spoki::net::icmp&) const {
    return target_key{p.saddr, p.ipid == 54321 || p.ttl > 200};
  }
  target_key operator()(const spoki::net::tcp& x) const {
    bool scanner_like = false;
    if (p.ipid == 54321 || p.ttl > 200 || x.options.empty())
      scanner_like = true;
    return target_key{p.saddr, scanner_like};
  }
  target_key operator()(const spoki::net::udp&) const {
    return target_key{p.saddr, p.ipid == 54321 || p.ttl > 200};
  }
  const packet& p;
};

struct vis_five_tuple_builder {
  net::five_tuple operator()(const spoki::net::icmp&) const {
    return net::five_tuple{spoki::net::protocol::icmp, p.saddr, p.daddr, 0, 0};
  }
  net::five_tuple operator()(const spoki::net::tcp& x) const {
    return net::five_tuple{spoki::net::protocol::tcp, p.saddr, p.daddr, x.sport,
                           x.dport};
  }
  net::five_tuple operator()(const spoki::net::udp& x) const {
    return net::five_tuple{spoki::net::protocol::udp, p.saddr, p.daddr, x.sport,
                           x.dport};
  }
  const packet& p;
};

// -- packet member impl -------------------------------------------------------

std::time_t packet::unix_ts() const {
  using namespace std::chrono;
  auto ts = duration_cast<seconds>(observed.time_since_epoch()).count();
  return static_cast<std::time_t>(ts);
}

target_key packet::get_key() const {
  return caf::visit(vis_key_builder{*this}, proto);
}

net::five_tuple packet::five_tuple() const {
  return caf::visit(vis_five_tuple_builder{*this}, proto);
}

std::string to_string(const packet& x) {
  std::stringstream s;
  s << "packet(saddr " << to_string(x.saddr) << ", daddr " << to_string(x.daddr)
    << ", ipid " << x.ipid << ", ttl " << static_cast<uint32_t>(x.ttl)
    << ", observed " << to_count(x.observed) << ", "
    << caf::visit(vis_stringify(), x.proto) << ")";
  return s.str();
}

// -- JSON ---------------------------------------------------------------------

/// Get the protocol enum value.
struct vis_protocol_string {
  std::string operator()(const spoki::net::icmp&) const {
    return "icmp";
  }
  std::string operator()(const spoki::net::tcp&) const {
    return "tcp";
  }
  std::string operator()(const spoki::net::udp&) const {
    return "udp";
  }
};

struct vis_loggify {
  template <class T>
  std::string operator()(const T& x) const {
    return make_log_string(x);
  }
};

void to_json(json& j, const packet& x) {
  j = json{{"saddr", to_string(x.saddr)},
           {"daddr", to_string(x.daddr)},
           {"ipid", x.ipid},
           {"ttl", x.ttl},
           {"observed", to_count(x.observed)},
           {"protocol", caf::visit(vis_protocol_string{}, x.proto)},
           {caf::visit(vis_protocol_string{}, x.proto), x.proto}};
}

void from_json(const json&, packet&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}

} // namespace spoki
