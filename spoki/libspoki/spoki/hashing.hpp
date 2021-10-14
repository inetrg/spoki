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

#include "spoki/config.hpp"

SPOKI_PUSH_WARNINGS
#include <libtrace_parallel.h>
SPOKI_POP_WARNINGS

#include <caf/actor.hpp>
#include <caf/hash/fnv.hpp>
#include <caf/ipv4_address.hpp>

#include "spoki/crc.hpp"
#include "spoki/net/protocol.hpp"

namespace spoki {

// Hashing for simple libtrace routing.
namespace trace {

/// Returns a static value. Used for parallel libtrace with a single thread.
uint64_t static_hash(const libtrace_packet_t* packet, void* data);

/// Hash addresses into max buckets using mod.
uint64_t modulo_hash(uint32_t addr, size_t max);

} // namespace trace

// -- custom 32 bit hash -------------------------------------------------------

/// Lookups in the consistent hash map for routing are performed using the
/// hash functions below in namespace spoki. These only hash to a 32 bit value.
/// Using std::hash gives us a 64 bit space which is too large when hashing
/// 32 bit integers (ipv4 addresses) and leads to an uneven usage of the hash
/// space. The crc32c hash yields better results (so far).
template <class T>
struct hash {
  using result_type = uint32_t;
  inline result_type operator()(const T& x) const {
    // This can probably be solved with more style ...
    static_assert(std::is_integral<T>::value,
                  "missing specialilzation for type");
    auto h = crc_init();
    h = crc_update(h, static_cast<const void*>(&x), sizeof(x));
    return static_cast<uint32_t>(crc_finalize(h));
  }
};

/// Specialization for std::string.
template <>
struct hash<std::string> {
  using result_type = uint32_t;
  inline result_type operator()(const std::string& x) const {
    auto h = crc_init();
    h = crc_update(h, x.data(), x.size());
    return static_cast<uint32_t>(crc_finalize(h));
  }
};

/// Specialization for caf::ipv4_address.
template <>
struct hash<caf::ipv4_address> {
  using result_type = uint32_t;
  inline result_type operator()(const caf::ipv4_address& addr) const {
    auto h = crc_init();
    h = crc_update(h, addr.data().data(), addr.num_bytes);
    return static_cast<uint32_t>(crc_finalize(h));
  }
};

/// Specialization for caf::actor.
template <>
struct hash<caf::actor> {
  using result_type = uint32_t;
  inline result_type operator()(const caf::actor& ref) const {
    auto id = ref.id();
    //auto node = ref.node()->hash_code();
    auto h = crc_init();
    h = crc_update(h, static_cast<const void*>(&id), sizeof(id));
    //h = crc_update(h, static_cast<const void*>(&node), sizeof(node));
    return static_cast<uint32_t>(crc_finalize(h));
  }
};

/// Combine `key` into the hash of `seed`. Used to hash std::pair below.
template <typename T>
void hash_combine(std::size_t& seed, const T& value) {
  std::hash<T> hasher;
  seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

} // namespace spoki

// -- implement std::hash for unordered_map ------------------------------------

namespace std {

/// Hash for caf::ipv4_address, enables usage as key in std::unordered_map.
template <>
struct hash<caf::ipv4_address> {
  inline size_t operator()(const caf::ipv4_address& addr) const {
    return std::hash<uint32_t>{}(addr.bits());
  }
};

template <>
struct hash<spoki::net::protocol> {
  size_t operator()(const spoki::net::protocol& proto) const {
    using utype = typename std::underlying_type<spoki::net::protocol>::type;
    std::hash<utype> hasher;
    return hasher(static_cast<utype>(proto));
  }
};

template <class LHS, class RHS>
struct hash<std::pair<LHS, RHS>> {
  size_t operator()(const std::pair<LHS, RHS>& value) const {
    size_t seed = 0;
    spoki::hash_combine(seed, value.first);
    spoki::hash_combine(seed, value.second);
    return seed;
  }
};

template <class T>
struct hash<std::vector<T>> {
  size_t operator()(const vector<T>& x) const {
    return caf::hash::fnv<size_t>::compute(x);
  }
};

} // namespace std
