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
#include "spoki/analysis/consistency.hpp"

#include <ctime>
#include <iomanip>
#include <iostream>

#include <algorithm>
#include <limits>
#include <numeric>

#include "spoki/analysis/classification.hpp"
#include "spoki/analysis/regression.hpp"

namespace {

// About an 8th of the IP ID space, max distance between trigger and probes
// to be considered genuine.
constexpr uint16_t ipid_treshold = 8000;
constexpr double min_interval = 10;

// The prediction errror threshhold is computed depending on the IPID estimate.
// This requires a few ICMP specific settings, see thesis 5.4.2
constexpr double icmp_sl = -0.225;
constexpr double icmp_cl = -35.0;
constexpr double icmp_sh = 0.03;
constexpr double icmp_ch = 75.0;

// TCP specific values.
constexpr double tcp_sl = -0.175;
constexpr double tcp_cl = -100.0;
constexpr double tcp_sh = 0.1;
constexpr double tcp_ch = 200.0;

// UDP specific values.
constexpr double udp_sl = 0.0;
constexpr double udp_cl = -250.0;
constexpr double udp_sh = 0.0;
constexpr double udp_ch = 350.0;

// Calculate IP id velocity between to data points.
double vel(const spoki::packet& lhs, const spoki::packet& rhs) {
  using namespace std::chrono;
  auto d = spoki::analysis::dist(lhs.ipid, rhs.ipid);
  auto t = duration_cast<milliseconds>(rhs.observed - lhs.observed).count();
  return double(d) / t;
}

double upper_bound(double dE, spoki::net::protocol proto) {
  switch (proto) {
    case spoki::net::protocol::icmp:
      return icmp_sh * dE + icmp_ch;
    case spoki::net::protocol::tcp:
      return tcp_sh * dE + tcp_ch;
    case spoki::net::protocol::udp:
      return udp_sh * dE + udp_ch;
    default:
      std::cerr << "unsupported protocol: " << spoki::net::to_string(proto)
                << std::endl;
      return 0.0;
  }
}

double lower_bound(double dE, spoki::net::protocol proto) {
  switch (proto) {
    case spoki::net::protocol::icmp:
      return icmp_sl * dE + icmp_cl;
    case spoki::net::protocol::tcp:
      return tcp_sl * dE + tcp_cl;
    case spoki::net::protocol::udp:
      return udp_sl * dE + udp_cl;
    default:
      std::cerr << "unsupported protocol: " << spoki::net::to_string(proto)
                << std::endl;
      return 0.0;
  }
}

bool check_interval(uint16_t actual, double prediction, double interval) {
  uint16_t upred = static_cast<uint16_t>(prediction);
  uint16_t uival = static_cast<uint16_t>(interval);

  uint16_t upper = upred + uival;
  uint16_t lower = upred - uival;

  // Should work, see this link:
  // https://fgiesen.wordpress.com/2015/09/24/intervals-in-modular-arithmetic/
  return (actual - lower) <= (upper - lower);

  // Should do the same as the above.
  //  if (upper > lower) {
  //    return upper >= actual && lower <= actual;
  //  } else {
  //    return upper >= actual || lower >= actual;
  //  }
}

} // namespace

namespace spoki::analysis {

std::vector<double> velocities(const std::vector<packet>& dps) {
  if (dps.empty())
    return {};
  std::vector<double> res(dps.size() - 1);
  std::transform(begin(dps), --end(dps), ++begin(dps), begin(res), vel);
  return res;
}

double mean_velocity(const std::vector<packet>& dps) {
  auto vels = velocities(dps);
  if (vels.empty())
    return 0.0;
  double total = std::accumulate(begin(vels), end(vels), 0.0);
  return total / vels.size();
}

double velocity(const std::vector<packet>& dps) {
  auto& r1 = dps.front();
  auto& r5 = dps.back();
  return vel(r1, r5);
}

bool monotonicity_test(const task& ev) {
  return !ev.packets.empty() && ev.type == classification::monotonic;
}

bool threshold_test(const task& ev) {
  auto tr = ev.initial.ipid;
  auto pr = ev.packets.front().ipid;
  if (tr <= pr)
    return dist(tr, pr) < ipid_treshold;
  else // if (trigger > probe)
    return dist(pr, tr)
           > (std::numeric_limits<uint16_t>::max() - ipid_treshold);
}

long delta_t(const task& ev) {
  using namespace std::chrono;
  return duration_cast<milliseconds>(ev.packets.back().observed
                                     - ev.initial.observed)
    .count();
}

double delta_E(double velocity, long dt) {
  return velocity * dt;
}

uint16_t delta_A(const task& ev) {
  return dist(ev.initial.ipid, ev.packets.back().ipid);
}

namespace consistency {

bool thesis(task& ev) {
  using namespace std::chrono;
  if (!monotonicity_test(ev))
    return false;
  if (!threshold_test(ev))
    return false;
  const auto v = velocity(ev.packets);
  const auto dt = delta_t(ev);
  const auto dE = delta_E(v, dt);
  const auto dA = delta_A(ev);
  const auto prediction_error = dA - dE;
  const auto proto = caf::visit(vis_protocol_type{}, ev.initial.proto);
  if (lower_bound(dE, proto) <= prediction_error
      && prediction_error <= upper_bound(dE, proto))
    return true;
  return false;
}

bool regression(task& ev) {
  using namespace std::chrono;
  // Monotonicity test.
  if (ev.packets.empty() || ev.type != classification::monotonic)
    return false;
  // Threshold test.
  if (!threshold_test(ev))
    return false;
  std::vector<double> xs;
  std::vector<double> ys;
  xs.reserve(ev.packets.size());
  ys.reserve(ev.packets.size());
  auto first_ts = ev.initial.observed;
  for (auto& p : ev.packets) {
    xs.emplace_back(duration_cast<milliseconds>(p.observed - first_ts).count());
    ys.emplace_back(p.ipid);
  }
  double m, b;
  std::tie(m, b) = spoki::analysis::least_squares_method(xs, ys);
  std::vector<double> predictions(xs.size());
  std::transform(std::begin(xs), std::end(xs), std::begin(predictions),
                 [m, b](const int x) { return m * x + b; });
  auto x_prediction = 0.0;
  auto sigma = spoki::analysis::sigma_for_prediction(ys, predictions);
  auto std_err = spoki::analysis::std_err_for_prediciton(xs, x_prediction,
                                                         sigma);
  auto interval = spoki::analysis::prediction_interval(std_err, 3.18);
  interval = std::max(min_interval, interval);
  // Yes, x is 0. Just want to avoid errors if things change.
  auto prediction = m * x_prediction + b;
  while (prediction > std::numeric_limits<uint16_t>::max())
    prediction -= std::numeric_limits<uint16_t>::max();
  while (prediction < 0)
    prediction += std::numeric_limits<uint16_t>::max();
  if (interval > std::numeric_limits<uint16_t>::max() / 4)
    return false;
  // With x equal to the trigger time (0) m is canceled out, leaving b as y.
  return check_interval(ev.initial.ipid, prediction, interval);
}

} // namespace consistency
} // namespace spoki::analysis
