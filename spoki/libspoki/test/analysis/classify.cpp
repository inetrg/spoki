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

#define SUITE classify
#include "test.hpp"

#include <cstdint>
#include <vector>

#include "spoki/analysis/classify.hpp"

#include "spoki/packet.hpp"

using spoki::packet;

namespace {

packet with_ipid(uint16_t ipid) {
  packet pkt;
  pkt.ipid = ipid;
  return pkt;
}

} // namespace

TEST(distances) {
  std::vector<packet> packets = {
    with_ipid(60),    with_ipid(600), with_ipid(6000),
    with_ipid(60000), with_ipid(60),
  };
  auto dists = spoki::analysis::ipid_distances(packets);
  CHECK_EQUAL(dists[0], 540);
  CHECK_EQUAL(dists[1], 5400);
  CHECK_EQUAL(dists[2], 54000);
  CHECK_EQUAL(dists[3], std::numeric_limits<uint16_t>::max() - 60000 + 60);
}

TEST(trivial classifier) {
  spoki::task t;
  // Unchecked: The timeserie's length is less than two.
  t.packets = {
    with_ipid(60),
  };
  t.num_probes = 4;
  CHECK_EQUAL(spoki::analysis::classifier::trivial(t),
              spoki::analysis::classification::unchecked);
  // All the IPID delats are zero.
  const uint16_t c = 60;
  t.packets = {
    with_ipid(c),
    with_ipid(c),
    with_ipid(c),
    with_ipid(c),
  };
  CHECK_EQUAL(spoki::analysis::classifier::trivial(t),
              spoki::analysis::classification::constant);
  // At least on IPID delat is greater than the random threshold (20000).
  t.packets = {
    with_ipid(c),
    with_ipid(c),
    with_ipid(c),
    with_ipid(c + 20000 + 1),
  };
  CHECK_EQUAL(spoki::analysis::classifier::trivial(t),
              spoki::analysis::classification::random);
  // All the IPID delats are less than a tolerance value.
  t.packets = {
    with_ipid(0),
    with_ipid(1),
    with_ipid(2),
    with_ipid(3),
  };
  CHECK_EQUAL(spoki::analysis::classifier::trivial(t),
              spoki::analysis::classification::monotonic);
  // Should work with wrap.
  t.packets = {
    with_ipid(55000),
    with_ipid(60000),
    with_ipid(65000),
    with_ipid(4000),
  };
  CHECK_EQUAL(spoki::analysis::classifier::trivial(t),
              spoki::analysis::classification::monotonic);
  // Greater than the tolerance value, smaller than random threshold.
  t.packets = {
    with_ipid(c),
    with_ipid(c),
    with_ipid(c),
    with_ipid(c + 20000),
  };
  CHECK_EQUAL(spoki::analysis::classifier::trivial(t),
              spoki::analysis::classification::other);
}

TEST(midar-- classifier) {
  spoki::task t;
  // Unchecked: Fewer than 75% of all probes got a response.
  t.packets = {
    with_ipid(60),
    with_ipid(600),
  };
  t.num_probes = 4;
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::unchecked);
  t.packets = {
    with_ipid(60),
    with_ipid(600),
    with_ipid(601),
  };
  t.num_probes = 5;
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::unchecked);
  // Degenerate: more than 25% replies have constant ipid.
  t.packets = {
    with_ipid(30),
    with_ipid(30),
    with_ipid(40),
    with_ipid(50),
  };
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::constant);
  // All the IPID delats are less than a tolerance value, ~30% of IP space.
  t.packets = {
    with_ipid(0),
    with_ipid(1),
    with_ipid(2),
    with_ipid(3),
  };
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::monotonic);
  // Should work with wrap.
  t.packets = {
    with_ipid(55000),
    with_ipid(60000),
    with_ipid(65000),
    with_ipid(4000),
  };
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::monotonic);
  uint16_t tol
    = static_cast<uint16_t>(std::numeric_limits<uint16_t>::max() * 0.3);
  t.packets = {
    with_ipid(1),
    with_ipid(1200),
    with_ipid(3000),
    with_ipid(static_cast<uint16_t>(3000 + tol)),
  };
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::monotonic);
  // One deltas is bigger than tolerance value.
  t.packets = {
    with_ipid(1),
    with_ipid(1200),
    with_ipid(3000),
    with_ipid(static_cast<uint16_t>(3000 + tol + 1)),
  };
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::other);
  // Just not monotonic.
  t.packets = {
    with_ipid(1200),
    with_ipid(1),
    with_ipid(3000),
    with_ipid(static_cast<uint16_t>(3000 + tol)),
  };
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::other);
  // Just not monotonic.
  t.packets = {with_ipid(5), with_ipid(4), with_ipid(3), with_ipid(2)};
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::other);
  // Easily monotonic.
  t.packets = {with_ipid(2), with_ipid(3), with_ipid(4), with_ipid(5)};
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::monotonic);
  // Too far away.
  t.packets = {with_ipid(2), with_ipid(3), with_ipid(4), with_ipid(50000)};
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::other);
  // Too far away.
  t.packets = {with_ipid(2), with_ipid(300), with_ipid(4), with_ipid(2000)};
  CHECK_EQUAL(spoki::analysis::classifier::midarmm(t),
              spoki::analysis::classification::other);
}
