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

#include "spoki/cache/entry.hpp"
#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"
#include "spoki/time.hpp"

namespace spoki {
namespace cache {

/// A cache to store probing results. Each address is store with the time stamp
/// of the most recent probe time and a bool that tags if we believe the host to
/// be genuine (`true`) of spoofed (`false`).
class SPOKI_CORE_EXPORT store {
  // -- allow serialization by CAF ---------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, store& x);

public:
  // -- member types -----------------------------------------------------------

  using key_type = caf::ipv4_address;
  using mapped_type = entry;
  using internal_container_type = std::unordered_map<key_type, mapped_type>;
  using value_type = internal_container_type::value_type;
  using size_type = internal_container_type::size_type;

  // -- constructors and assignment operators ----------------------------------

  store() = default;
  store(store&&) = default;
  store(const store&) = default;

  store& operator=(store&&) = default;
  store& operator=(const store&) = default;

  // -- member access ----------------------------------------------------------

  /// Merge `other` cache into this one.
  void merge(const store& other);

  /// Merge an entry into this cache using the key `addr`. In case an entry with
  /// the same key exists, the conflict is resolved based on the timestamp in
  /// the added entry. Newer entries persist. In case of equal timestamps the
  /// associated bools will be combined with a logic 'and'.
  void merge(const key_type& addr, const mapped_type& e);

  /// Returns `true` if the cache contains an entry for `addr`.
  bool contains(const key_type& addr) const;

  /// Read-only access to cache entries. Insertions and updates should be
  /// performed via `merge`.
  const mapped_type& operator[](const key_type& addr) const;

  // -- other member functions -------------------------------------------------

  /// Returns the number of entries in the cache.
  inline size_type size() const {
    return data_.size();
  }

  /// Remove all elements satisfying `pred`.
  template <class Pred>
  void remove_if(Pred pred) {
    for (auto it = data_.begin(); it != data_.end();) {
      if (pred(*it))
        it = data_.erase(it);
      else
        ++it;
    }
  }

private:
  /// Initialize cache with a copy of data.
  store(const internal_container_type& data);

  /// Actual store data.
  internal_container_type data_;

  /// Default entry for operator[].
  static const entry default_;
};

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, store& x) {
  return f.object(x).fields(f.field("data", x.data_));
}

} // namespace cache
} // namespace spoki
