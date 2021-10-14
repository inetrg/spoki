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

#include "spoki/cache/rotating_store.hpp"

namespace {

constexpr auto no_duration = std::chrono::time_point<
  std::chrono::system_clock, std::chrono::milliseconds>::duration(0);

} // namespace

namespace spoki::cache {

rotating_store::rotating_store()
  : data_(1), default_{timestamp{no_duration}, false} {
  // nop
}

void rotating_store::insert(const caf::ipv4_address& addr, const entry& e) {
  // TODO: Before first full rotation, insert data into random store.
  data_.front()[addr] = e;
}

bool rotating_store::contains(const caf::ipv4_address& addr) const {
  return std::any_of(std::begin(data_), std::end(data_),
                     [&addr](const internal_container_type& d) {
                       return d.count(addr) > 0;
                     });
}

const entry& rotating_store::operator[](const caf::ipv4_address& addr) const {
  for (auto& d : data_) {
    if (d.count(addr) > 0)
      return d.at(addr);
  }
  return default_;
}

rotating_store::size_type rotating_store::size() const {
  size_type s = 0;
  for (auto& d : data_)
    s += d.size();
  return s;
}

void rotating_store::rotate(size_t max) {
  data_.insert(std::begin(data_), internal_container_type());
  if (data_.size() > max)
    data_.pop_back();
}

} // namespace spoki::cache
