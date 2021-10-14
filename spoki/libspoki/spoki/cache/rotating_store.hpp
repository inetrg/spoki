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

#include <unordered_map>

#include <caf/ipv4_address.hpp>
#include <caf/meta/type_name.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/cache/entry.hpp"
#include "spoki/hashing.hpp"
#include "spoki/time.hpp"

namespace spoki::cache {

/// A cache to store spoofing suspicions that allows rotating out values
/// subsets.
class SPOKI_CORE_EXPORT rotating_store {
  // -- allows serialization by CAF --------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 rotating_store& x);

public:
  // -- member types -----------------------------------------------------------

  using key_type = caf::ipv4_address;
  using mapped_type = entry;
  using internal_container_type = std::unordered_map<key_type, mapped_type>;
  using value_type = internal_container_type::value_type;
  using size_type = internal_container_type::size_type;

  // -- constructors and assignment operators ----------------------------------
  rotating_store();
  rotating_store(rotating_store&&) = default;
  rotating_store(const rotating_store&) = default;

  rotating_store& operator=(rotating_store&&) = default;
  rotating_store& operator=(const rotating_store&) = default;

  /// Insert an entry `e` under key `addr`.
  void insert(const key_type& addr, const mapped_type& e);

  /// Returns `true` if the cache contains an entry for key `addr`.
  bool contains(const key_type& addr) const;

  /// Read-only access to cache entries. Insertions and updates should be
  /// performed via `update`.
  const mapped_type& operator[](const key_type& addr) const;

  /// Returns the number of entries in the cache.
  size_type size() const;

  /// Rotate stored data. Drops the all subsets beyond the `max` newest ones.
  void rotate(size_t max = 4);

private:
  std::vector<internal_container_type> data_;

  /// Default entry for operator[] if no value is found in the store.
  entry default_;
};

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, rotating_store& x) {
  return f.object(x).fields(f.field("data", x.data_));
}

} // namespace spoki::cache
