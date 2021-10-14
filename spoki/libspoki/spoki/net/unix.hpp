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

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <caf/meta/type_name.hpp>

#include <nlohmann/json_fwd.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/hashing.hpp"
#include "spoki/net/tcp_opt.hpp"

namespace spoki::net {

// -- networking ---------------------------------------------------------------

int connect(std::string name);

// Same as TCP.
// int read(int sock, void* buf, size_t len);
// int write(int sock, const void* buf, size_t len);

} // namespace std
