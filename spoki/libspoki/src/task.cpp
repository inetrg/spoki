/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2021
 * Authors: Raphael Hiesgen
 *
 * All rights reserved.
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#include "spoki/task.hpp"

#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

#include "spoki/analysis/classification.hpp"
#include "spoki/config.hpp"
#include "spoki/probe/payloads.hpp"

// For convenience.
using json = nlohmann::json;

namespace spoki {

namespace {

constexpr auto separator = ",";

} // namespace

task::task(packet initial, std::vector<packet> packets)
  : initial(initial),
    type(analysis::classification::unchecked),
    consistent(false),
    suspected_scanner(false),
    last_anum(0),
    num_probes(0),
    packets(std::move(packets)) {
  // nop
}

std::string to_string(const task& e) {
  std::stringstream ss;
  ss << "task(" << separator << to_string(e.initial) << separator
     << to_string(e.type) << separator
     << (e.consistent ? "consistent" : "unknown") << separator
     << (e.suspected_scanner ? "suspicious" : "regular") << separator
     << e.num_probes;
  for (auto& p : e.packets)
    ss << separator << to_string(p);
  ss << ")";
  return ss.str();
}

// -- JSON ---------------------------------------------------------------------

void to_json(json& j, const task& x) {
  j = json{
    {"initial", x.initial},       {"classification", to_string(x.type)},
    {"valid", x.consistent},      {"suspected_scanner", x.suspected_scanner},
    {"num_probes", x.num_probes}, {"packets", x.packets}};
}

void from_json(const json&, task&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}

} // namespace spoki
