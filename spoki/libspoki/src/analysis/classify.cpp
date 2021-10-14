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

#include "spoki/analysis/classify.hpp"

#include <algorithm>
#include <unordered_map>

namespace {

// Threshold to detect random behavior of IP IDs.
// TODO: Apparently taken from nmap, needs some verification.
constexpr uint16_t random_threshold = 20000;

// Allowed delta to classify IP ID series as monotonic;
constexpr uint16_t tolerance_value
  = static_cast<uint16_t>(std::numeric_limits<uint16_t>::max() * 0.3);

// Minimum number of events required to classify them.
constexpr auto min_events = size_t{2};

} // namespace

namespace spoki::analysis {

std::vector<uint16_t> ipid_distances(const std::vector<packet>& pkts) {
  if (pkts.empty())
    return {};
  std::vector<uint16_t> res(pkts.size() - 1);
  auto diff = [&](const packet& lhs, const packet& rhs) {
    auto distance = rhs.ipid - lhs.ipid;
    if (lhs.ipid > rhs.ipid)
      distance += std::numeric_limits<uint16_t>::max();
    return distance;
  };
  std::transform(begin(pkts), --end(pkts), ++begin(pkts), begin(res), diff);
  return res;
}

/*
std::vector<uint16_t> response_distances(const data_vec& dps) {
  if (dps.empty())
    return {};
  std::vector<uint16_t> res(dps.size());
  auto diff = [&](const data_point& dp) {
    auto diff = dp.probe_ipid - dp.reply_ipid;
    if (dp.reply_ipid > dp.probe_ipid)
      diff += std::numeric_limits<uint16_t>::max();
    return diff;
  };
  std::transform(begin(dps), end(dps), begin(res), diff);
  return res;
}
*/

namespace classifier {

classification trivial(const task& ev) {
  if (ev.packets.size() < min_events)
    return classification::unchecked;

  auto id_diffs = ipid_distances(ev.packets);
  if (all_of(begin(id_diffs), end(id_diffs),
             [](size_t val) { return val == 0; }))
    return classification::constant;

  if (any_of(begin(id_diffs), end(id_diffs),
             [](uint16_t val) { return val > random_threshold; }))
    return classification::random;

  if (none_of(begin(id_diffs), end(id_diffs),
              [](uint16_t val) { return val > tolerance_value; }))
    return classification::monotonic;

  return classification::other;
}

classification midarmm(const task& ev) {
  // Check min events, ...
  if (ev.packets.size() < min_events)
    return classification::unchecked;

  // ... and the midar-specific threshold.
  auto threshold = static_cast<size_t>((ev.num_probes * 3.0 / 4.0) + 0.5);
  if (ev.packets.size() < threshold)
    return classification::unchecked;

  // Drop if 25% of packets are degenrate (reply == probe or constant)
  threshold = static_cast<size_t>((ev.packets.size() * 1.0 / 4.0) + 0.5);
  //  size_t degenerate = 0;
  //  // Count reply == probe
  //  for (auto& dp : ev.data_points)
  //    if (dp.ipid == dp.probe_ipid)
  //      ++degenerate;
  //  if (degenerate > threshold)
  //    return classification::constant;

  // Count constant. TODO: Optimize this.
  std::unordered_map<uint16_t, size_t> hist;
  for (auto& pkt : ev.packets)
    hist[pkt.ipid] += 1;
  for (auto& p : hist)
    if (p.second > threshold)
      return classification::constant;

  auto id_diffs = ipid_distances(ev.packets);
  // TODO: Look at MIDAR which also takes into account limited precision
  // counters, hosts with IDs shorter than 16 bit, little-endian ordering,
  // carry anomalies. (Look thesis 3.2.1 MIDAR--)
  if (none_of(begin(id_diffs), end(id_diffs),
              [](uint16_t val) { return val > tolerance_value; }))
    return classification::monotonic;

  return classification::other;
}

} // namespace classifier
} // namespace spoki::analysis
