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

#include <cstdint>
#include <string>
#include <vector>

#include <caf/ipv4_address.hpp>

#include <nlohmann/json_fwd.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/packet.hpp"
#include "spoki/probe/method.hpp"
#include "spoki/target_key.hpp"

namespace spoki::probe {

struct SPOKI_CORE_EXPORT request {
  method probe_method;
  caf::ipv4_address saddr;
  caf::ipv4_address daddr;
  uint16_t sport;
  uint16_t dport;
  uint32_t snum;
  uint32_t anum;
  uint32_t user_id;
  std::vector<char> payload;
  uint16_t num_probes;
};

SPOKI_CORE_EXPORT std::string make_command(const request& req);


SPOKI_CORE_EXPORT std::string make_tcp_synack_probe_pe(const request& req);
SPOKI_CORE_EXPORT std::string make_tcp_synack_probe_ss(const request& req);

// -- JSON ---------------------------------------------------------------------

SPOKI_CORE_EXPORT void to_json(nlohmann::json& j, const request& x);

SPOKI_CORE_EXPORT void from_json(const nlohmann::json&, request&);

// -- Serialization ------------------------------------------------------------

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, request& x) {
  return f.object(x).fields(
    f.field("probe_method", x.probe_method), f.field("saddr", x.saddr),
    f.field("daddr", x.daddr), f.field("sport", x.sport),
    f.field("dport", x.dport), f.field("snum", x.snum), f.field("anum", x.anum),
    f.field("payload", x.payload), f.field("num_probes", x.num_probes));
}

} // namespace spoki::probe
