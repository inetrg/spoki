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

#include <vector>

#include <caf/all.hpp>

#include "spoki/fwd.hpp"

CAF_BEGIN_TYPE_ID_BLOCK(spoki_core, first_custom_type_id)

  // -- types ------------------------------------------------------------------

  CAF_ADD_TYPE_ID(spoki_core, (spoki::operation))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::packet))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::task))
  // CAF_ADD_TYPE_ID(spoki_core, (spoki::timeout))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::target_key))

  CAF_ADD_TYPE_ID(spoki_core, (spoki::cache::entry))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::cache::tcp_timeout))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::cache::icmp_timeout))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::cache::udp_timeout))

  CAF_ADD_TYPE_ID(spoki_core, (spoki::net::icmp))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::net::five_tuple))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::net::tcp))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::net::udp))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::net::udp_hdr))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::net::icmp_type))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::net::protocol))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::net::tcp_opt))

  CAF_ADD_TYPE_ID(spoki_core, (spoki::probe::method))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::probe::request))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::probe::udp_request))

  CAF_ADD_TYPE_ID(spoki_core, (spoki::scamper::statistics))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::scamper::timepoint))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::scamper::ec))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::scamper::reply))

  CAF_ADD_TYPE_ID(spoki_core, (spoki::trace::result))
  CAF_ADD_TYPE_ID(spoki_core, (spoki::trace::tally))

  CAF_ADD_TYPE_ID(spoki_core, (std::vector<char>) )
  CAF_ADD_TYPE_ID(spoki_core, (std::vector<unsigned char>) )
  CAF_ADD_TYPE_ID(spoki_core, (std::vector<std::string>) )
  CAF_ADD_TYPE_ID(spoki_core, (std::vector<spoki::packet>) )
  CAF_ADD_TYPE_ID(spoki_core, (std::vector<caf::ipv4_address>) )

  // -- atoms ------------------------------------------------------------------

  CAF_ADD_ATOM(spoki_core, spoki, broker_atom)
  CAF_ADD_ATOM(spoki_core, spoki, cleanup_atom)
  CAF_ADD_ATOM(spoki_core, spoki, collect_atom)
  CAF_ADD_ATOM(spoki_core, spoki, collect_icmp_atom)
  CAF_ADD_ATOM(spoki_core, spoki, collect_tcp_atom)
  CAF_ADD_ATOM(spoki_core, spoki, collect_udp_atom)
  CAF_ADD_ATOM(spoki_core, spoki, connect_atom)
  CAF_ADD_ATOM(spoki_core, spoki, done_atom)
  CAF_ADD_ATOM(spoki_core, spoki, event_atom)
  CAF_ADD_ATOM(spoki_core, spoki, failed_atom)
  CAF_ADD_ATOM(spoki_core, spoki, flush_atom)
  CAF_ADD_ATOM(spoki_core, spoki, get_atom)
  CAF_ADD_ATOM(spoki_core, spoki, idle_atom)
  CAF_ADD_ATOM(spoki_core, spoki, method_atom)
  CAF_ADD_ATOM(spoki_core, spoki, probed_atom)
  CAF_ADD_ATOM(spoki_core, spoki, probes_atom)
  CAF_ADD_ATOM(spoki_core, spoki, reconnect_atom)
  CAF_ADD_ATOM(spoki_core, spoki, report_atom)
  CAF_ADD_ATOM(spoki_core, spoki, request_atom)
  CAF_ADD_ATOM(spoki_core, spoki, reset_atom)
  CAF_ADD_ATOM(spoki_core, spoki, result_atom)
  CAF_ADD_ATOM(spoki_core, spoki, rotate_atom)
  CAF_ADD_ATOM(spoki_core, spoki, shard_atom)
  CAF_ADD_ATOM(spoki_core, spoki, start_atom)
  CAF_ADD_ATOM(spoki_core, spoki, stats_atom)
  CAF_ADD_ATOM(spoki_core, spoki, stop_atom)
  CAF_ADD_ATOM(spoki_core, spoki, success_atom)
  CAF_ADD_ATOM(spoki_core, spoki, tick_atom)
  CAF_ADD_ATOM(spoki_core, spoki, trace_atom)

CAF_END_TYPE_ID_BLOCK(spoki_core)

namespace spoki::detail {

static constexpr caf::type_id_t spoki_core_begin = caf::first_custom_type_id;

static constexpr caf::type_id_t spoki_core_end = caf::id_block::spoki_core::end;

} // namespace spoki::detail
