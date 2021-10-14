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

#include <sstream>

#include "spoki/cache/entry.hpp"

namespace spoki::cache {

std::string to_string(const entry& e) {
  std::stringstream ss;
  using namespace std::chrono;
  auto ep = time_point_cast<milliseconds>(e.ts).time_since_epoch();
  auto in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(ep);
  ss << in_ms.count() << ", " << (e.consistent ? "true" : "false");
  return ss.str();
}

} // namespace spoki::cache
