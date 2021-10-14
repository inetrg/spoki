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

#define SUITE cache.store
#include "test.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

#include "spoki/cache/entry.hpp"
#include "spoki/cache/store.hpp"
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

FIXTURE_SCOPE(store_tests, fixture)

TEST(insertion) {
  using namespace spoki::cache;
  entry a{make_test_ts(1), true};
  entry b{make_test_ts(2), true};
  entry c{make_test_ts(0), false};
  CHECK_EQUAL(c.ts.time_since_epoch().count(), 0);
  CHECK_EQUAL(c.consistent, false);
  store store;
  CHECK_EQUAL(store.size(), 0ul);
  store.merge(foo, a);
  CHECK_EQUAL(store.size(), 1ul);
  store.merge(bar, b);
  CHECK_EQUAL(store.size(), 2ul);
  CHECK_EQUAL(store[foo].ts, a.ts);
  CHECK_EQUAL(store[foo], a);
  CHECK_EQUAL(store[bar], b);
  CHECK_EQUAL(store.size(), 2ul);
  // Unlike a standard map this should not create an entry.
  CHECK_EQUAL(store[baz], c);
  CHECK_EQUAL(store.size(), 2ul);
}

TEST(merge caches) {
  using namespace spoki::cache;
  entry a{make_test_ts(1), true};
  entry b{make_test_ts(2), true};
  entry c{make_test_ts(3), false};
  store store_a;
  store store_b;
  store store_c;
  store_a.merge(foo, a);
  store_a.merge(bar, b);
  CHECK_EQUAL(store_a.size(), 2ul);
  store_b.merge(foo, a);
  store_b.merge(baz, c);
  CHECK_EQUAL(store_b.size(), 2ul);
  CHECK_EQUAL(store_c.size(), 0ul);
  store_c.merge(store_a);
  CHECK_EQUAL(store_c.size(), 2ul);
  store_c.merge(store_b);
  CHECK_EQUAL(store_c.size(), 3ul);
}

TEST(merge conflicts) {
  using namespace spoki::cache;
  entry a{make_test_ts(2), true};
  // Same time stamp, conflicting bool.
  entry conflict_1{make_test_ts(2), false};
  // Time stamp with later value.
  entry conflict_2{make_test_ts(3), true};
  // Time stamp with earlier value.
  entry conflict_3{make_test_ts(1), false};
  // Ensure that false is adapted as well.
  entry conflict_4{make_test_ts(4), false};
  store store;
  store.merge(foo, a);
  CHECK_EQUAL(store.size(), 1ul);
  CHECK_EQUAL(store[foo].ts, make_test_ts(2));
  CHECK_EQUAL(store[foo].consistent, true);
  store.merge(foo, conflict_1);
  CHECK_EQUAL(store.size(), 1ul);
  CHECK_EQUAL(store[foo].ts, make_test_ts(2));
  CHECK_EQUAL(store[foo].consistent, false);
  store.merge(foo, conflict_2);
  CHECK_EQUAL(store.size(), 1ul);
  CHECK_EQUAL(store[foo].ts, make_test_ts(3));
  CHECK_EQUAL(store[foo].consistent, true);
  store.merge(foo, conflict_3);
  CHECK_EQUAL(store.size(), 1ul);
  CHECK_EQUAL(store[foo].ts, make_test_ts(3));
  CHECK_EQUAL(store[foo].consistent, true);
  store.merge(foo, conflict_4);
  CHECK_EQUAL(store.size(), 1ul);
  CHECK_EQUAL(store[foo].ts, make_test_ts(4));
  CHECK_EQUAL(store[foo].consistent, false);
}

TEST(cleanup) {
  using namespace spoki::cache;
  // This is a not a test. The diff set is not implemented properly
  // and should always return the full cache.
  entry a{make_test_ts(1), true};
  entry b{make_test_ts(2), true};
  store store;
  store.merge(foo, a);
  store.merge(bar, b);
  store.merge(baz, a);
  CHECK_EQUAL(store.size(), 3ul);
  store.remove_if([](const store::value_type& val) {
    return val.second.ts != make_test_ts(2);
  });
  CHECK_EQUAL(store.size(), 1ul);
  CHECK_EQUAL(store[bar], b);
  store.merge(foo, a);
  store.merge(baz, a);
  store.remove_if(
    [&](const store::value_type& val) { return val.first == bar; });
  CHECK_EQUAL(store.size(), 2ul);
  CHECK(store.contains(foo));
  CHECK(store.contains(baz));
}

FIXTURE_SCOPE_END()
