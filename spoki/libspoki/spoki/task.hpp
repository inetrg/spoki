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
#include <vector>

#include <caf/ipv4_address.hpp>
#include <caf/meta/type_name.hpp>
#include <caf/variant.hpp>

#include <nlohmann/json_fwd.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/fwd.hpp"
#include "spoki/packet.hpp"

namespace spoki {

/// An event bundles the information of a trigger with the data points of the
/// probes and a classitifacion for the probes sequence and a consistency
/// evaluation.
struct SPOKI_CORE_EXPORT task {
  // -- constructors -----------------------------------------------------------

  task(packet initial = {}, std::vector<packet> data_points = {});

  task(const task&) = default;
  task(task&&) = default;
  task& operator=(task&&) = default;
  task& operator=(const task&) = default;

  // -- members ----------------------------------------------------------------

  /// The observation that triggered this task.
  packet initial;

  /// Classification of the IP id sequence in the events. (Only for ICMP & UDP.)
  analysis::classification type;

  /// Consistency assumption, either IP id check or handshake spoofing.
  bool consistent;

  /// Depending on the characteristics of the original packet we might suspect
  /// it to be sent by a scanner.
  bool suspected_scanner;

  /// Only used to reset TCP connections.
  uint32_t last_anum;

  /// Number of probes send to the target.
  uint32_t num_probes;

  /// Events we collected in response to out probing.
  std::vector<packet> packets;
};

/// Print a task.
SPOKI_CORE_EXPORT std::string to_string(const task& t);

// -- JSON ---------------------------------------------------------------------

void to_json(nlohmann::json& j, const task& x);

void from_json(const nlohmann::json&, task&);

// -- CAF serialization --------------------------------------------------------

/// Enable serialization by CAF.
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, task& x) {
  return f.object(x).fields(f.field("initial", x.initial),
                            f.field("type", x.type),
                            f.field("consistent", x.consistent),
                            f.field("suspected_scanner", x.suspected_scanner),
                            f.field("last_anum", x.last_anum),
                            f.field("num_probes", x.num_probes),
                            f.field("packets", x.packets));
}

} // namespace spoki
