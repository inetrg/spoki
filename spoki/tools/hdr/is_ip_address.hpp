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

#pragma once

#include <chrono>
#include <string>

namespace spoki {

bool is_ip_address(const std::string& addr);

bool is_valid_host(const std::string& addr);

} // namespace spoki
