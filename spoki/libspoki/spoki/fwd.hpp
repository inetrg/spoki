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

#include <caf/variant.hpp>

#include "spoki/detail/core_export.hpp"

namespace spoki {

// -- templates ----------------------------------------------------------------

template <class>
struct hash;

template <class T, class Hash = spoki::hash<T>,
          class Key = typename Hash::result_type,
          class Compare = std::less<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>>
class consistent_hash_map;

// -- enumerations -------------------------------------------------------------

/// Forward declaration.
enum class operation;

// -- classes ------------------------------------------------------------------

// -- structs ------------------------------------------------------------------

struct asc;
struct out_file;
struct collector_state;
struct packet;
struct target_key;
struct task;

// -- aliases ------------------------------------------------------------------

// using timestamp = caf::timestamp;
// using timespan = caf::timespan;

// -- analysis classes ---------------------------------------------------------

namespace analysis {

enum class classification : uint8_t;

} // namespace analysis

// -- cache classes ------------------------------------------------------------

namespace cache {

struct entry;
struct icmp_timeout;
struct shard_state;
struct tcp_timeout;
struct udp_timeout;

class rotating_store;
class store;

using timeout = caf::variant<icmp_timeout, tcp_timeout, udp_timeout>;

} // namespace cache

// -- net classes --------------------------------------------------------------

namespace net {

struct endpoint;
struct icmp;
struct five_tuple;
struct tcp;
struct udp;
struct udp_hdr;

enum class icmp_type : uint8_t;
enum class protocol : uint8_t;
enum class tcp_opt : uint8_t;

} // namespace net

// -- probe classes ------------------------------------------------------------

namespace probe {

struct request;
struct udp_request;

class udp_prober;

enum class method : uint8_t;

} // namespace probe

// -- scamper classses ---------------------------------------------------------

namespace scamper {

struct broker_state;
struct instance;
struct reply;
struct statistics;
struct timepoint;

class async_decoder;

enum class ec : uint8_t;

} // namespace scamper

// -- trace classses -----------------------------------------------------------

namespace trace {

struct global;
struct local;
struct reader_state;
struct result;
struct tally;

class instance;

} // namespace trace

} // namespace spoki
