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

#include <string>

namespace spoki {

/// Socket operation to control the async decoder event subscriptions.
enum class operation { read, write };

/// Stringify the operation.
inline std::string to_string(operation op) {
  return op == operation::read ? "read" : "write";
}

} // namespace spoki
