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
#include <unordered_map>
#include <vector>

#include <caf/meta/type_name.hpp>

#include <nlohmann/json_fwd.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"
#include "spoki/net/tcp_opt.hpp"

namespace spoki::net {

/// TCP event info.
struct SPOKI_CORE_EXPORT tcp {
  uint16_t sport;
  uint16_t dport;
  uint32_t snum;
  uint32_t anum;
  /*
  uint8_t syn : 1;
  uint8_t ack : 1;
  uint8_t rst : 1;
  uint8_t fin : 1;
  */
  bool syn;
  bool ack;
  bool rst;
  bool fin;
  uint16_t window_size;
  std::unordered_map<tcp_opt, caf::optional<uint32_t>> options;
  std::vector<char> payload;
};

/// Equality comparison operator for tcp events.
inline bool operator==(const tcp& lhs, const tcp& rhs) {
  return lhs.sport == rhs.sport && lhs.dport == rhs.dport
         && lhs.snum == rhs.snum && lhs.anum == rhs.anum && lhs.syn == rhs.syn
         && lhs.ack == rhs.ack && lhs.rst == rhs.rst && lhs.fin == rhs.fin
         && lhs.window_size == rhs.window_size;
}

/// Print data of a tcp event.
SPOKI_CORE_EXPORT std::string to_string(const tcp& x);

/// Make a string with the recorded TCP flags.
SPOKI_CORE_EXPORT std::string tcp_flags_str(const tcp& x);

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, tcp& x) {
  return f.object(x).fields(f.field("sport", x.sport),
                            f.field("dport", x.dport), f.field("snum", x.snum),
                            f.field("anum", x.anum), f.field("syn", x.syn),
                            f.field("ack", x.ack), f.field("rst", x.rst),
                            f.field("window_size", x.window_size),
                            f.field("options", x.options),
                            f.field("payload", x.payload));
}

// -- networking ---------------------------------------------------------------

int connect(std::string host, uint16_t port);

int read(int sock, void* buf, size_t len);

int write(int sock, const void* buf, size_t len);

// -- JSON ---------------------------------------------------------------------

void to_json(nlohmann::json& j, const tcp& x);

void from_json(const nlohmann::json&, tcp&);

} // namespace spoki::net

// -- HASH ---------------------------------------------------------------------

namespace std {

template <>
struct hash<spoki::net::tcp> {
  size_t operator()(const spoki::net::tcp& x) const noexcept {
    size_t seed = 0;
    spoki::hash_combine(seed, x.sport);
    spoki::hash_combine(seed, x.dport);
    spoki::hash_combine(seed, x.snum);
    spoki::hash_combine(seed, x.anum);
    spoki::hash_combine(seed, x.window_size);
    return seed;
  }
};

} // namespace std
