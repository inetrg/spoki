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

#define SUITE consistent_hash_map
#include "test.hpp"

#include "spoki/consistent_hash_map.hpp"
#include "spoki/hashing.hpp"

TEST(insert and delete) {
  spoki::consistent_hash_map<std::string> chm;
  CHECK_EQUAL(chm.size(), 0ul);
  MESSAGE("insert a value");
  chm.insert("hello");
  CHECK_EQUAL(chm.size(), 1ul);
  auto itr = chm.find("hello");
  CHECK(itr != chm.end());
  CHECK_EQUAL(itr->second, "hello");
  MESSAGE("insert another value");
  chm.insert("world");
  CHECK_EQUAL(chm.size(), 2ul);
  itr = chm.find("world");
  CHECK(itr != chm.end());
  CHECK_EQUAL(itr->second, "world");
  MESSAGE("check if out containers contains all expected values");
  CHECK(chm.contains("hello"));
  CHECK(chm.contains("world"));
  CHECK(!chm.contains("foo"));
  MESSAGE("check if find works as expected");
  itr = chm.find("world");
  chm.erase(itr);
  CHECK_EQUAL(chm.size(), 1ul);
  CHECK(chm.contains("hello"));
  CHECK(!chm.contains("world"));
  itr = chm.find("world");
  CHECK_EQUAL(itr, chm.end());
  MESSAGE("erase some values");
  itr = chm.find("hello");
  chm.erase(itr);
  CHECK(!chm.contains("hello"));
  CHECK_EQUAL(chm.size(), 0ul);
  CHECK(chm.empty());
  MESSAGE("insert and clear");
  chm.insert("hello");
  chm.insert("world");
  CHECK_EQUAL(chm.size(), 2ul);
  chm.clear();
  CHECK_EQUAL(chm.size(), 0ul);
}

TEST(iterators) {
  using chm_type = spoki::consistent_hash_map<std::string>;
  chm_type chm;
  chm.insert("foo");
  chm.insert("bar");
  chm.insert("baz");
  std::vector<std::pair<chm_type::key_type, chm_type::mapped_type>> items;
  MESSAGE("check if we can iterate our container");
  for (auto& p : chm)
    items.push_back(p);
  CHECK(items.size(), 3ul);
  for (auto& p : items)
    CHECK(chm.contains(p.second));
  auto items_ritr = items.rbegin();
  MESSAGE("check reverse iteration");
  for (auto it = chm.rbegin(); it != chm.rend(); /* nop */) {
    CHECK_EQUAL(it->first, items_ritr->first);
    CHECK_EQUAL(it->second, items_ritr->second);
    items_ritr++;
    it++;
  }
}

TEST(ring neighbors) {
  using chm_type = spoki::consistent_hash_map<std::string>;
  chm_type chm;
  chm.insert("foo");
  chm.insert("bar");
  chm.insert("baz");
  MESSAGE("record order (works due to underlying std::map)");
  std::vector<std::pair<chm_type::key_type, chm_type::mapped_type>> items;
  for (auto& p : chm)
    items.push_back(p);
  const auto& front = items[0].second;
  const auto& middle = items[1].second;
  const auto& back = items[2].second;
  MESSAGE("traverse clockwise");
  auto next = chm.next(front);
  REQUIRE_EQUAL(next.size(), 1ul);
  CHECK_EQUAL(*next.begin(), middle);
  next = chm.next(middle);
  REQUIRE_EQUAL(next.size(), 1ul);
  CHECK_EQUAL(*next.begin(), back);
  next = chm.next(back);
  REQUIRE_EQUAL(next.size(), 1ul);
  CHECK_EQUAL(*next.begin(), front);
  MESSAGE("traverse counterclockwise");
  auto prev = chm.previous(back);
  REQUIRE_EQUAL(prev.size(), 1ul);
  CHECK_EQUAL(*prev.begin(), middle);
  prev = chm.previous(middle);
  REQUIRE_EQUAL(prev.size(), 1ul);
  CHECK_EQUAL(*prev.begin(), front);
  prev = chm.previous(front);
  REQUIRE_EQUAL(prev.size(), 1ul);
  CHECK_EQUAL(*prev.begin(), back);
  MESSAGE("next n");
  next = chm.next(middle, 2ul);
  REQUIRE_EQUAL(next.size(), 2ul);
  CHECK(next.find(back) != next.end());
  CHECK(next.find(front) != next.end());
  CHECK(next.find(middle) == next.end());
  next = chm.next(back, 2ul);
  REQUIRE_EQUAL(next.size(), 2ul);
  CHECK(next.find(back) == next.end());
  CHECK(next.find(front) != next.end());
  CHECK(next.find(middle) != next.end());
  next = chm.next(front, 2ul);
  REQUIRE_EQUAL(next.size(), 2ul);
  CHECK(next.find(back) != next.end());
  CHECK(next.find(front) == next.end());
  CHECK(next.find(middle) != next.end());
  next = chm.next(middle, 3ul);
  REQUIRE_EQUAL(next.size(), 2ul);
  CHECK(next.find(back) != next.end());
  CHECK(next.find(front) != next.end());
  CHECK(next.find(middle) == next.end());
  MESSAGE("previous n");
  prev = chm.previous(middle, 2ul);
  REQUIRE_EQUAL(prev.size(), 2ul);
  CHECK(prev.find(back) != prev.end());
  CHECK(prev.find(front) != prev.end());
  CHECK(prev.find(middle) == prev.end());
  prev = chm.previous(back, 2ul);
  REQUIRE_EQUAL(prev.size(), 2ul);
  CHECK(prev.find(back) == prev.end());
  CHECK(prev.find(front) != prev.end());
  CHECK(prev.find(middle) != prev.end());
  prev = chm.previous(front, 2ul);
  REQUIRE_EQUAL(prev.size(), 2ul);
  CHECK(prev.find(back) != prev.end());
  CHECK(prev.find(front) == prev.end());
  CHECK(prev.find(middle) != prev.end());
  prev = chm.previous(middle, 3ul);
  REQUIRE_EQUAL(prev.size(), 2ul);
  CHECK(prev.find(back) != prev.end());
  CHECK(prev.find(front) != prev.end());
  CHECK(prev.find(middle) == prev.end());
  MESSAGE("avoid duplicate values");
  chm.insert(std::make_pair(std::hash<std::string>{}(front + "2"), front));
  CHECK_EQUAL(chm.size(), 4ul);
  next = chm.next(front, 4ul);
  REQUIRE_EQUAL(next.size(), 2ul);
  CHECK(next.find(back) != next.end());
  CHECK(next.find(front) == next.end());
  CHECK(next.find(middle) != next.end());
  next = chm.next(middle, 4ul);
  REQUIRE_EQUAL(next.size(), 2ul);
  CHECK(next.find(back) != next.end());
  CHECK(next.find(front) != next.end());
  CHECK(next.find(middle) == next.end());
  next = chm.next(back, 4ul);
  REQUIRE_EQUAL(next.size(), 2ul);
  CHECK(next.find(back) == next.end());
  CHECK(next.find(front) != next.end());
  CHECK(next.find(middle) != next.end());
  prev = chm.previous(front, 4ul);
  REQUIRE_EQUAL(prev.size(), 2ul);
  CHECK(prev.find(back) != prev.end());
  CHECK(prev.find(front) == prev.end());
  CHECK(prev.find(middle) != prev.end());
  prev = chm.previous(middle, 4ul);
  REQUIRE_EQUAL(prev.size(), 2ul);
  CHECK(prev.find(back) != prev.end());
  CHECK(prev.find(front) != prev.end());
  CHECK(prev.find(middle) == prev.end());
  prev = chm.previous(back, 4ul);
  REQUIRE_EQUAL(prev.size(), 2ul);
  CHECK(prev.find(back) == prev.end());
  CHECK(prev.find(front) != prev.end());
  CHECK(prev.find(middle) != prev.end());
}

TEST(bucket lookup) {
  using chm_type = spoki::consistent_hash_map<std::string>;
  chm_type chm;
  std::vector<std::pair<chm_type::key_type, chm_type::mapped_type>> items;
  auto init = [&]() {
    MESSAGE("resetting state");
    chm.clear();
    items.clear();
    chm.insert("foo");
    chm.insert("bar");
    chm.insert("baz");
    for (auto& p : chm)
      items.push_back(p);
  };
  auto front = [&]() { return items[0].second; };
  auto middle = [&]() { return items[1].second; };
  auto back = [&]() { return items[2].second; };
  MESSAGE("get bucket for exact value");
  init();
  auto ebucket = chm.resolve(front());
  REQUIRE(ebucket);
  CHECK_EQUAL(*ebucket, front());
  ebucket = chm.resolve(middle());
  REQUIRE(ebucket);
  CHECK_EQUAL(*ebucket, middle());
  ebucket = chm.resolve(back());
  REQUIRE(ebucket);
  CHECK_EQUAL(*ebucket, back());
  MESSAGE("get bucket for middle value");
  init();
  auto itr = chm.find(middle());
  REQUIRE(itr != chm.end());
  chm.erase(itr);
  ebucket = chm.resolve(middle());
  REQUIRE(ebucket);
  CHECK_EQUAL(*ebucket, back());
  MESSAGE("get bucket with wrap around");
  init();
  itr = chm.find(back());
  REQUIRE(itr != chm.end());
  chm.erase(itr);
  ebucket = chm.resolve(back());
  REQUIRE(ebucket);
  CHECK_EQUAL(*ebucket, front());
}

TEST(count values) {
  spoki::consistent_hash_map<std::string> chm;
  const auto value = "foo";
  const auto other = "bar";
  chm.insert(std::make_pair(uint32_t{3}, value));
  CHECK_EQUAL(chm.count(value), 1ul);
  chm.insert(std::make_pair(uint32_t{4}, value));
  CHECK_EQUAL(chm.count(value), 2ul);
  chm.insert(std::make_pair(uint32_t{5}, other));
  CHECK_EQUAL(chm.count(value), 2ul);
  CHECK_EQUAL(chm.count(other), 1ul);
}
