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

#include <nlohmann/json_fwd.hpp>

namespace spoki::net {

/// Just the UDP header.
struct udp_hdr {
  uint16_t sport;
  uint16_t dport;
  uint16_t length;
  uint16_t chksum;
};

// -- JSON ---------------------------------------------------------------------

void to_json(nlohmann::json& j, const udp_hdr& x);

void from_json(const nlohmann::json&, udp_hdr&);

// -- Serialization ------------------------------------------------------------

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, udp_hdr& x) {
  return f.object(x).fields(f.field("sport", x.sport),
                            f.field("sport", x.dport),
                            f.field("length", x.length),
                            f.field("chksum", x.chksum));
}

} // namespace spoki::net
