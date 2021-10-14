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

#include "spoki/detail/core_export.hpp"
#include "spoki/probe/method.hpp"

namespace spoki::scamper {

struct SPOKI_CORE_EXPORT statistics {
  uint32_t replies;
  uint32_t loss;
};

struct SPOKI_CORE_EXPORT timepoint {
  long sec;
  long usec;
};

struct SPOKI_CORE_EXPORT reply {
  std::string type;
  float version;
  probe::method probe_method;
  std::string src;
  std::string dst;
  timepoint start;
  uint32_t ping_sent;
  uint32_t probe_size;
  uint32_t userid;
  uint32_t ttl;
  uint32_t wait;
  uint32_t timeout;
  uint16_t sport;
  uint16_t dport;
  std::string payload;
  std::vector<std::string> flags;
  std::vector<char> responses;
  statistics stats;
};

// -- logging ------------------------------------------------------------------

SPOKI_CORE_EXPORT std::string to_log_line(const reply& repl,
                                          char delimiter = '|');

// -- serialization ------------------------------------------------------------

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, statistics& x) {
  return f.object(x).fields(f.field("replies", x.replies),
                            f.field("loss", x.loss));
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, timepoint& x) {
  return f.object(x).fields(f.field("sec", x.sec), f.field("usec", x.usec));
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, reply& x) {
  return f.object(x).fields(
    f.field("type", x.type), f.field("version", x.version),
    f.field("method", x.probe_method), f.field("src", x.src),
    f.field("dst", x.dst), f.field("start", x.start),
    f.field("ping_sent", x.ping_sent), f.field("probe_size", x.probe_size),
    f.field("userid", x.userid), f.field("ttl", x.ttl), f.field("wait", x.wait),
    f.field("timeout", x.timeout), f.field("sport", x.sport),
    f.field("dport", x.dport), f.field("payload", x.payload),
    f.field("flags", x.flags), f.field("responses", x.responses),
    f.field("stats", x.stats));
}

} // namespace spoki::scamper
