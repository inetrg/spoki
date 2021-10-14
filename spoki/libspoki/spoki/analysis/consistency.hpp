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

#include "spoki/detail/core_export.hpp"
#include "spoki/packet.hpp"
#include "spoki/task.hpp"

namespace spoki::analysis {

/// Calculate the distance from lhs to rhs, assuming rhs happened after lhs.
/// The wrap for 16 bits is taken into account.
SPOKI_CORE_EXPORT inline uint16_t dist(uint16_t lhs, uint16_t rhs) {
  uint16_t res = rhs - lhs;
  if (rhs < lhs)
    res += std::numeric_limits<uint16_t>::max();
  return res;
}

/// -- Only exposed for testing ------------------------------------------------

/// Calculate the velocity using the first and last entry in `dps`.
SPOKI_CORE_EXPORT double velocity(const std::vector<packet>& dps);

/// Calculates pair-wise velocities of the IP ids in `dps`.
SPOKI_CORE_EXPORT std::vector<double>
velocities(const std::vector<packet>& dps);

/// Mean velocity for an array of `data_points`.
SPOKI_CORE_EXPORT double mean_velocity(const std::vector<packet>& dps);

/// Check if an event was classified as monotonic.
/// (Does NOT perform the classification itself.)
SPOKI_CORE_EXPORT bool monotonicity_test(const task& ev);

/// Check if the distance between the trigger and first probe is within the
/// threshold.
SPOKI_CORE_EXPORT bool threshold_test(const task& ev);

/// Time between the trigger arrival and the arrival of the last probe.
SPOKI_CORE_EXPORT long delta_t(const task& ev);

/// Estimate IP space pased in `dt` based on `velocity`.
/// Shouldn't this return an uint16_t?
SPOKI_CORE_EXPORT double delta_E(double velocity, long dt);

/// Actual distance from trigger to last probe for event `ev`.
SPOKI_CORE_EXPORT uint16_t delta_A(const task& ev);

namespace consistency {

/// Evaluate event `ev` and return `true` if the trigger is consistent with
/// the probes, `false` otherwise. Named thesis because the algorithm is
/// present in the thesis of Alessandro Puccetti, 2015.
SPOKI_CORE_EXPORT bool thesis(task& ev);

/// Evaluate event `ev` and return `true` if the trigger is consistent with
/// the probes, `false` otherwise. Based on regression analysis and the
/// prediciton interval.
SPOKI_CORE_EXPORT bool regression(task& ev);

} // namespace consistency
} // namespace spoki::analysis
