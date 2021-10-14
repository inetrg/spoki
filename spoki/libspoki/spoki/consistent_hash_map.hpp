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

#include <functional>
#include <map>
#include <set>

#include <caf/meta/type_name.hpp>
#include <caf/optional.hpp>

#include "spoki/fwd.hpp"
#include "spoki/hashing.hpp"

namespace spoki {

/// A consistent hash map orders its values of type `T` on a ring as buckets and
/// enables lookup of buckets based on the hash of a given element. The bucket
/// is determined by traversing the ring to the next available value. Per
/// default the hash implementation in spoki::hash is used. Furthermore a value
/// may be added under a given key of type `Key` to enable adding a bucket
/// multiple time and thus balancing lookups more evently across the hash space.
/// Note: Unlike a normal hash map functions often expect the mapped_type
/// instead of the key_type.
template <class T, class Hash, class Key, class Compare, class Allocator>
class consistent_hash_map {
public:
  // -- member types -----------------------------------------------------------

  using key_type = Key;
  using mapped_type = T;
  using map_type = std::map<key_type, mapped_type, Compare, Allocator>;
  using value_type = typename map_type::value_type;
  using size_type = typename map_type::size_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using allocator_type = Allocator;
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  using const_pointer = typename std::allocator_traits<
    Allocator>::const_pointer;
  using iterator = typename map_type::iterator;
  using const_iterator = typename map_type::const_iterator;
  using reverse_iterator = typename map_type::reverse_iterator;
  using const_reverse_iterator = typename map_type::const_reverse_iterator;

  // -- constructors, assignment operators -------------------------------------

  consistent_hash_map() = default;

  consistent_hash_map(const consistent_hash_map& other) = default;

  consistent_hash_map(consistent_hash_map&& other) = default;

  consistent_hash_map& operator=(const consistent_hash_map& other) = default;

  consistent_hash_map& operator=(consistent_hash_map&& other) = default;

  // -- member access ----------------------------------------------------------

  /// Insert a new `x` into the container as a bucket. Hash will be used
  /// to determine the key.
  std::pair<iterator, bool> insert(const mapped_type& x) {
    return ring_.insert(value_type(hash_(x), x));
  }

  /// Insert a new `x` into the container as a bucket. Hash will be used
  /// to determine the key.
  std::pair<iterator, bool> insert(mapped_type&& x) {
    auto key = hash_(x);
    return ring_.insert(value_type(key, std::forward<mapped_type>(x)));
  }

  /// Insert a new `x` consisting of a std::pair<key_type, mapped_type>.
  std::pair<iterator, bool> insert(const value_type& x) {
    return ring_.insert(x);
  }

  /// Insert a new `x` consisting of a std::pair<key_type, mapped_type>.
  std::pair<iterator, bool> insert(value_type&& x) {
    return ring_.insert(std::forward<value_type>(x));
  }

  /// Erase the element at `pos`.
  iterator erase(const_iterator pos) {
    return ring_.erase(pos);
  }

  /// Erase the elements in [`first`; `last`).
  iterator erase(const_iterator first, const_iterator last) {
    return ring_.erase(first, last);
  }

  // -- lookup -----------------------------------------------------------------

  /// Returns an iterator to the position of `x` or end();
  iterator find(const mapped_type& x) {
    return ring_.find(hash_(x));
  }

  /// Returns an iterator to a pair containing `x` or end().
  const_iterator find(const mapped_type& x) const {
    return ring_.find(hash_(x));
  }

  /// Returns true if the hash of `x` matches a key in the container.
  bool contains(const mapped_type& x) const {
    return ring_.find(hash_(x)) != ring_.end();
  }

  /// Returns the number elements that map to `x`.
  size_type count(const mapped_type& x) {
    size_type matching = 0;
    for (auto& p : ring_)
      if (p.second == x)
        ++matching;
    return matching;
  }

  /// Returns an iterator to the first element not less than the given `key`.
  template <class K, class H = spoki::hash<K>>
  iterator lower_bound(const K& key) {
    auto h = H{}(key);
    return ring_.lower_bound(h);
  }

  /// Returns an iterator to the first element not less than the given `key`.
  template <class K, class H = spoki::hash<K>>
  const_iterator lower_bound(const K& key) const {
    auto h = H{}(key);
    return ring_.lower_bound(h);
  }

  /// Returns an iterator to the first element greater than the given `key`.
  template <class K, class H = spoki::hash<K>>
  iterator upper_bound(const K& key) {
    auto h = H{}(key);
    return ring_.upper_bound(h);
  }

  /// Returns an iterator to the first element greater than the given `key` .
  template <class K, class H = spoki::hash<K>>
  const_iterator upper_bound(const K& key) const {
    auto h = H{}(key);
    return ring_.upper_bound(h);
  }

  /// Find the first (up to) `n` buckets for `x`.
  std::set<mapped_type> next(const mapped_type& x, size_t n = 1) const {
    std::set<mapped_type> result;
    if (ring_.empty())
      return result;
    const auto origin = hash_(x);
    const auto end = ring_.end();
    auto itr = ring_.upper_bound(origin);
    if (itr == end)
      itr = ring_.begin();
    while (itr->first != origin && result.size() < n) {
      if (itr->second != x)
        result.insert(itr->second);
      itr++;
      if (itr == end)
        itr = ring_.begin();
    }
    return result;
  }

  /// Find the first (up to) `n` buckets for `x`.
  std::set<mapped_type> previous(const mapped_type& x, size_t n = 1) const {
    std::set<mapped_type> result;
    if (ring_.empty())
      return result;
    const auto origin = hash_(x);
    const auto begin = ring_.begin();
    auto itr = ring_.lower_bound(origin);
    if (itr == ring_.begin())
      itr = ring_.end();
    itr--;
    while (itr->first != origin && result.size() < n) {
      if (itr->second != x)
        result.insert(itr->second);
      if (itr == begin)
        itr = ring_.end();
      itr--;
    }
    return result;
  }

  /// Find the bucket for `key` according to the hash by `H`.
  template <class K, class H = spoki::hash<K>>
  caf::optional<const mapped_type&> resolve(const K& key) const {
    if (ring_.empty())
      return caf::none;
    auto itr = ring_.lower_bound(H{}(key));
    if (itr == ring_.end())
      itr = ring_.begin();
    return itr->second;
  }

  // -- iterators --------------------------------------------------------------

  inline iterator begin() noexcept {
    return ring_.begin();
  }

  inline iterator end() noexcept {
    return ring_.end();
  }

  inline const_iterator begin() const noexcept {
    return ring_.begin();
  }

  inline const_iterator end() const noexcept {
    return ring_.end();
  }

  inline const_iterator cbegin() noexcept {
    return ring_.cbegin();
  }

  inline const_iterator cend() noexcept {
    return ring_.cend();
  }

  inline reverse_iterator rbegin() noexcept {
    return ring_.rbegin();
  }

  inline reverse_iterator rend() noexcept {
    return ring_.rend();
  }

  inline const_reverse_iterator rbegin() const noexcept {
    return ring_.rbegin();
  }

  inline const_reverse_iterator rend() const noexcept {
    return ring_.rend();
  }

  inline const_reverse_iterator crbegin() const noexcept {
    return ring_.crbegin();
  }

  inline const_reverse_iterator crend() const noexcept {
    return ring_.crend();
  }

  // -- size -------------------------------------------------------------------

  /// Returns `true` if the container is empty, `false` otherwise.
  inline bool empty() const noexcept {
    return ring_.empty();
  }

  /// Returns the number of elements in the container.
  inline size_type size() const noexcept {
    return ring_.size();
  }

  /// Returns the maximum theoretically possible value of n, for which the
  /// call allocate(n, 0) could succeed.
  inline size_type max_size() const noexcept {
    return ring_.max_size();
  }

  // -- modifiers --------------------------------------------------------------

  /// Clear all entries in the container.
  inline void clear() noexcept {
    return ring_.clear();
  }

  /// Swap this with other.
  void swap(consistent_hash_map& other) {
    ring_.swap(other.ring_);
    hash_.swap(other.hash_);
  }

  /// Erase all entires mapping to `x`.
  void erase_all(const mapped_type& x) {
    for (auto it = ring_.begin(); it != ring_.end();) {
      if (it->second == x)
        it = ring_.erase(it);
      else
        it++;
    }
  }

private:
  Hash hash_;
  map_type ring_;
};

/// Enable serialization by CAF.
template <class Inspector, class T, class H, class R, class A>
typename Inspector::result_type inspect(Inspector& f,
                                        consistent_hash_map<T, H, R, A>& x) {
  return f.object(x).fields(f.field("ring", x.ring_));
}

} // namespace spoki
