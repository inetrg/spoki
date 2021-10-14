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

#include <caf/all.hpp>

#include "spoki/atoms.hpp"
#include "spoki/probe/request.hpp"

namespace perf {

struct counts {
  uint32_t requests;
  uint32_t stats;
  static const char* name;
};

caf::behavior counter(caf::stateful_actor<counts>* self);

caf::behavior aggregator(caf::stateful_actor<counts>* self, caf::actor cntr);

} // namespace perf
