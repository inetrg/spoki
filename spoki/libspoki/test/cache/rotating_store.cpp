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

#define SUITE cache.rotating_store
#include "test.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

#include "spoki/cache/entry.hpp"
#include "spoki/cache/rotating_store.hpp"
#include "spoki/time.hpp"

namespace {

spoki::timestamp make_test_ts(long val) {
  return spoki::timestamp{spoki::timestamp::duration{val}};
}

struct fixture {
  fixture()
    : foo(caf::ipv4_address::from_bits(23)),
      bar(caf::ipv4_address::from_bits(42)),
      baz(caf::ipv4_address::from_bits(1337)) {
    // nop
  }

  caf::ipv4_address foo;
  caf::ipv4_address bar;
  caf::ipv4_address baz;
};

} // namespace

FIXTURE_SCOPE(rotating_store_tests, fixture)

TEST(insertion) {
  using namespace spoki::cache;
  entry a{make_test_ts(1), true};
  entry b{make_test_ts(2), true};
  entry c{make_test_ts(0), false};
  CHECK_EQUAL(c.ts.time_since_epoch().count(), 0);
  CHECK_EQUAL(c.consistent, false);
  rotating_store rotating_store;
  CHECK_EQUAL(rotating_store.size(), 0ul);
  rotating_store.insert(foo, a);
  CHECK_EQUAL(rotating_store.size(), 1ul);
  rotating_store.insert(bar, b);
  CHECK_EQUAL(rotating_store.size(), 2ul);
  CHECK_EQUAL(rotating_store[foo], a);
  CHECK_EQUAL(rotating_store[bar], b);
  CHECK_EQUAL(rotating_store.size(), 2ul);
  // Unlike a standard map this should not create an entry.
  CHECK_EQUAL(rotating_store[baz], c);
  CHECK_EQUAL(rotating_store.size(), 2ul);
}

TEST(insertion with rotation) {
  using namespace spoki::cache;
  entry a{make_test_ts(1), true};
  entry b{make_test_ts(2), true};
  entry c{make_test_ts(0), false};
  CHECK_EQUAL(c.ts.time_since_epoch().count(), 0);
  CHECK_EQUAL(c.consistent, false);
  rotating_store rotating_store;
  CHECK_EQUAL(rotating_store.size(), 0ul);
  rotating_store.insert(foo, a);
  rotating_store.rotate();
  CHECK_EQUAL(rotating_store.size(), 1ul);
  rotating_store.insert(bar, b);
  rotating_store.rotate();
  CHECK_EQUAL(rotating_store.size(), 2ul);
  CHECK_EQUAL(rotating_store[foo], a);
  CHECK_EQUAL(rotating_store[bar], b);
  CHECK_EQUAL(rotating_store.size(), 2ul);
  // Unlike a standard map this should not create an entry.
  CHECK_EQUAL(rotating_store[baz], c);
  CHECK_EQUAL(rotating_store.size(), 2ul);
}

TEST(insertion with rotation and loss) {
  using namespace spoki::cache;
  entry a{make_test_ts(1), true};
  entry b{make_test_ts(2), true};
  // This is the default entry of the store and returned if no value exists.
  entry def{make_test_ts(0), false};
  CHECK_EQUAL(def.ts.time_since_epoch().count(), 0);
  CHECK_EQUAL(def.consistent, false);
  rotating_store rotating_store;
  CHECK_EQUAL(rotating_store.size(), 0ul);
  rotating_store.insert(foo, a);
  CHECK_EQUAL(rotating_store.size(), 1ul);
  // Now we have to subset in the store.
  rotating_store.rotate(2);
  CHECK_EQUAL(rotating_store.size(), 1ul);
  rotating_store.insert(bar, b);
  CHECK_EQUAL(rotating_store.size(), 2ul);
  rotating_store.rotate(2);
  // And we should still have two, but entry 'foo' was rotated out.
  CHECK_EQUAL(rotating_store.size(), 1ul);
  CHECK_EQUAL(rotating_store[foo], def);
  CHECK_EQUAL(rotating_store[bar], b);
  // Unlike a standard map this should not create an entry.
  CHECK_EQUAL(rotating_store[baz], def);
  CHECK_EQUAL(rotating_store.size(), 1ul);
}

FIXTURE_SCOPE_END()
