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
#include <unordered_map>
#include <vector>

namespace spoki::probe {

using payload_map = std::unordered_map<uint16_t, std::vector<char>>;
using payload_str_map = std::unordered_map<uint16_t, std::string>;

/// Returns a map from port to service specific probe as vec over ascii chars.
payload_map get_payloads();

/// Returns a map from port to service specific probe as a hex string.
payload_str_map get_payload_hex_strs();

/// Convert a buffer of characters to a hex string.
std::string to_hex_string(const std::vector<char>& buf);

} // namespace spoki::probe
