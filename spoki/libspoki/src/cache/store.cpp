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

#include <sstream>

#include "spoki/cache/store.hpp"

namespace {

constexpr auto no_duration = std::chrono::time_point<
  std::chrono::system_clock, std::chrono::milliseconds>::duration(0);

} // namespace

namespace spoki::cache {

const entry store::default_ = entry{timestamp{no_duration}, false};

store::store(const std::unordered_map<caf::ipv4_address, entry>& data)
  : data_{data} {
  // nop
}

void store::merge(const store& other) {
  for (auto& val : other.data_)
    merge(val.first, val.second);
}

void store::merge(const caf::ipv4_address& addr, const entry& e) {
  // TODO: Record changes to minimize change set.
  if (contains(addr)) {
    auto& existing = data_[addr];
    // Check time stamp and keep the newer one or merge for conflicts.
    if (existing.ts < e.ts)
      existing = e;
    else if (existing.ts == e.ts)
      existing.consistent = existing.consistent && e.consistent;
    // else: The existing one is newer, do nothing.
  } else {
    data_[addr] = e;
  }
}

bool store::contains(const caf::ipv4_address& addr) const {
  return data_.count(addr) > 0;
}

const entry& store::operator[](const caf::ipv4_address& addr) const {
  if (contains(addr))
    return data_.at(addr);
  return default_;
}

} // namespace spoki::cache
