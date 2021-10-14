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

#include "spoki/analysis/classification.hpp"
#include "spoki/detail/core_export.hpp"
#include "spoki/task.hpp"

namespace spoki::analysis {

/// Calculate pair-wise distance between ipids in the vector. Takes wrap into
/// account.
SPOKI_CORE_EXPORT std::vector<uint16_t>
ipid_distances(const std::vector<packet>& pkts);

namespace classifier {

/// A trivial algorithm to assign a `classification` to the IP ids in the given
/// event `ev`.
SPOKI_CORE_EXPORT classification trivial(const task& ev);

/// A alternative algorithm to assign a `classification` to the IP ids in the
/// given event `ev`. Roughly based on the algorithm used be Midar:
///  - https://www.caida.org/tools/measurement/midar/
SPOKI_CORE_EXPORT classification midarmm(const task& ev);

} // namespace classifier

} // namespace spoki::analysis
