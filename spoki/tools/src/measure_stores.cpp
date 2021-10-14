/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2019
 * Authors: Raphael Hiesgen
 *
 * All rights reserved.
 *
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

#include <caf/ipv4_address.hpp>

#include "spoki/cache/entry.hpp"
#include "spoki/cache/rotating_store.hpp"
#include "spoki/cache/store.hpp"
#include "spoki/time.hpp"

namespace {

spoki::timestamp make_test_ts(long val) {
  return spoki::timestamp{spoki::timestamp::duration{val}};
}

} // namespace

int main() {
  using val_vec = std::vector<std::pair<caf::ipv4_address, spoki::cache::entry>>;
  val_vec values;
  const auto num = 20000000;
  std::cout << "Creating " << num << " values for our test" << std::endl;
  for (int i = 0; i < num; ++i)
    values.push_back(std::make_pair(caf::ipv4_address::from_bits(i),
                                    spoki::cache::entry{make_test_ts(i), true}));
  const auto subset_size = 1000000;
  std::cout << "Separating values into subsets of " << (subset_size * 3)
            << std::endl;
  std::vector<val_vec> subsets;
  for (int i = 0; i < num; i += subset_size)
    subsets.push_back(
      val_vec(std::begin(values) + i, std::begin(values) + i + subset_size));
  for (auto& sub : subsets)
    if (sub.size() != subset_size)
      std::cout << "not the right size: " << sub.size() << std::endl;

  // Make larger, overlapping subsets
  for (size_t i = 0; i < (subsets.size() - 2); ++i) {
    subsets[i].insert(std::end(subsets[i]), std::begin(subsets[i + 1]),
                      std::end(subsets[i + 1]));
    subsets[i].insert(std::end(subsets[i]), std::begin(subsets[i + 2]),
                      std::end(subsets[i + 2]));
  }
  subsets.pop_back();
  subsets.pop_back();

  for (auto& sub : subsets)
    if (sub.size() != 3 * subset_size)
      std::cout << "not the right size: " << sub.size() << std::endl;

  std::cout << "Randomizing subsets" << std::endl;
  auto rng = std::default_random_engine{};
  for (auto& sub : subsets)
    std::shuffle(std::begin(sub), std::end(sub), rng);

  std::cout << "Starting ..." << std::endl;
  auto start = std::chrono::system_clock::now();
  /*
  spoki::cache::rotating_store rs;
  for (auto& s : subsets) {
    auto i = 0;
    for (auto& e : s) {
      ++i;
      if (!rs.contains(e.first))
        rs.insert(e.first, e.second);
      if (i % subset_size == 0)
        rs.rotate();
    }
  }
  */
  std::random_device
    rd; // Will be used to obtain a seed for the random number engine
  std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<> dis(1, 4);
  spoki::cache::store st;
  auto j = 0;
  for (auto& s : subsets) {
    auto i = 0;
    for (auto& e : s) {
      ++i;
      if (!st.contains(e.first))
        st.merge(e.first, e.second);
      if (i % subset_size == 0) {
        ++j;
        if (j > 4) {
          auto cmp = make_test_ts((j - 3) * subset_size);
          st.remove_if([&](const spoki::cache::store::value_type& val) {
            // val types: (ipv4_address, (timestamp, bool))
            return val.second.ts < cmp && dis(gen) > 1;
          });
        }
      }
    }
  }
  auto end = std::chrono::system_clock::now();
  auto diff = end - start;
  std::cout
    << "took: "
    << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
    << "ms" << std::endl;
}
