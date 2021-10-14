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

#include <cassert>
#include <iostream>
#include <sstream>

#include <caf/all.hpp>
#include <caf/ipv4_address.hpp>

#include <libtrace.h>

#include "spoki/time.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

// -- CLI ----------------------------------------------------------------------

class configuration : public actor_system_config {
public:
  string uri = "";
  configuration() {
    opt_group{custom_options_, "global"}.add(uri, "uri,u",
                                             "set uri for libtrace");
  }
};

// -- FILTER SETTINGS ----------------------------------------------------------

enum tree_flag {
  TREE_FLAG_UNFILTERED = 0x01,
  TREE_FLAG_NONSPOOFED = 0x02,
  TREE_FLAG_NONERRATIC = 0x04,
  TREE_FLAG_RFCCLEAN = 0x08,
};

// Need to define a list of tags. Easy.
struct tag_def {
  const char* name;
  int tree_flags;
  const char* bpf;
};

const std::vector<tag_def> tag_defs{
  // Unfiltered tag.
  {
    "all-pkts",
    TREE_FLAG_UNFILTERED,
    nullptr,
  },
  // Non-spoofed tag.
  {"abnormal-protocol",
   TREE_FLAG_NONSPOOFED | TREE_FLAG_NONERRATIC | TREE_FLAG_RFCCLEAN,
   "(icmp or udp or proto 41 or (tcp and ((tcp[tcpflags] & 0x2f )= tcp-syn or "
   "(tcp[tcpflags] & 0x2f) = tcp-ack or (tcp[tcpflags] & 0x2f) = tcp-rst or "
   "(tcp[tcpflags] & 0x2f) = tcp-fin or (tcp[tcpflags] & 0x2f) = "
   "(tcp-syn|tcp-fin) or (tcp[tcpflags] & 0x2f) = (tcp-syn|tcp-ack) or "
   "(tcp[tcpflags] & 0x2f) = (tcp-fin|tcp-ack) or (tcp[tcpflags] & 0x2f) = "
   "(tcp-ack|tcp-push) or (tcp[tcpflags] & 0x2f) = "
   "(tcp-ack|tcp-push|tcp-fin))))"},
  {
    "ttl-200",
    TREE_FLAG_NONSPOOFED | TREE_FLAG_NONERRATIC | TREE_FLAG_RFCCLEAN,
    "((ip[8] < 200) or icmp)",
  },
  {
    "fragmented-v2",
    TREE_FLAG_NONSPOOFED | TREE_FLAG_NONERRATIC | TREE_FLAG_RFCCLEAN,
    "((ip[6:2] & 0x9fff)=0)",
  },
  {
    "last-byte-src-0",
    TREE_FLAG_NONSPOOFED | TREE_FLAG_NONERRATIC | TREE_FLAG_RFCCLEAN,
    "(ip[15:1] != 0)",
  },
  {
    "last-byte-src-255",
    TREE_FLAG_NONSPOOFED | TREE_FLAG_NONERRATIC | TREE_FLAG_RFCCLEAN,
    "(ip[15:1] != 255)",
  },
  {
    "same-src-dst",
    TREE_FLAG_NONSPOOFED | TREE_FLAG_NONERRATIC | TREE_FLAG_RFCCLEAN,
    "ip[12:4] != ip[16:4]",
  },
  {
    "udp-port-0",
    TREE_FLAG_NONSPOOFED | TREE_FLAG_NONERRATIC | TREE_FLAG_RFCCLEAN,
    "not (udp port 0)",
  },
  {
    "tcp-port-0",
    TREE_FLAG_NONSPOOFED | TREE_FLAG_NONERRATIC | TREE_FLAG_RFCCLEAN,
    "not (tcp port 0)",
  },
  {
    "rfc5735",
    TREE_FLAG_NONSPOOFED | TREE_FLAG_NONERRATIC,
    "not (src net 0.0.0.0/8 or src net 10.0.0.0/8 or src net 127.0.0.0/8 or "
    "src net 169.254.0.0/16 or src net 172.16.0.0/12 or src net 192.0.0.0/24 "
    "or src net 192.0.2.0/24 or src net 192.88.99.0/24 or src net "
    "192.168.0.0/16 or src net 198.18.0.0/15 or src net 198.51.100.0/24 or src "
    "net 203.0.113.0/24 or src net 224.0.0.0/4 or src net 240.0.0.0/4)",
  },
};

// -- LIBTRACE PROCESSING ------------------------------------------------------

static int per_packet(libtrace_packet_t* packet, libtrace_filter_t* filter) {
  // Apply the filter to the packet.
  int ret = trace_apply_filter(filter, packet);
  // Check for any errors that occur during the filtering process.
  if (ret == -1) {
    cerr << "Error applying filter" << endl;
    return -1;
  }
  // If we get a return value of zero, the packet did not match the
  // filter so we want to return immediately.
  if (ret == 0)
    return 0;
  uint16_t ether_type;
  uint32_t remaining;
  auto layer3 = trace_get_layer3(packet, &ether_type, &remaining);
  if (layer3 == nullptr) {
    cout << "encountered packet without IP header" << endl;
    return 0;
  }
  switch (ether_type) {
    case 0x0800: {
      // auto tr_str = trace_get_source_address_string(packet, nullptr, 0);
      // std::cout << "trace says ip is: " << tr_str << std::endl;
      auto ip_hdr = reinterpret_cast<libtrace_ip_t*>(layer3);
      auto ip_id = static_cast<uint16_t>(ip_hdr->ip_id);
      auto tp = spoki::to_time_point(trace_get_timeval(packet));
      auto addr = caf::ipv4_address::from_bits(ip_hdr->ip_src.s_addr);
      cout << to_string(addr) << "," << tp.time_since_epoch().count() << ","
           << ip_id << endl;
      break;
    }
    case 0x86DD: {
      // IPv6
      break;
    }
    default:
      // Other.
      break;
  }
  return 0;
}

// The cleanup function has now been extended to destroy the filter and
// output trace as well.
static void libtrace_cleanup(libtrace_t* trace, libtrace_packet_t* packet,
                             libtrace_filter_t* filter) {
  if (trace)
    trace_destroy(trace);
  if (packet)
    trace_destroy_packet(packet);
  if (filter)
    trace_destroy_filter(filter);
}

void caf_main(actor_system&, const configuration& config) {
  if (config.uri.empty()) {
    std::cerr << "Please specify an URI to read from." << std::endl;
    return;
  }

  // Creating the packet structure.
  auto* packet = trace_create_packet();
  if (packet == nullptr) {
    perror("Creating libtrace packet");
    libtrace_cleanup(nullptr, packet, nullptr);
    return;
  }

  // Creating and starting the INPUT trace.
  auto* trace = trace_create(config.uri.c_str());
  if (trace_is_err(trace)) {
    trace_perror(trace, "Opening trace file");
    libtrace_cleanup(trace, packet, nullptr);
    return;
  }
  if (trace_start(trace) == -1) {
    trace_perror(trace, "Starting trace");
    libtrace_cleanup(trace, packet, nullptr);
    return;
  }

  stringstream filter_builder;
  for (auto& def : tag_defs) {
    if (!filter_builder.str().empty())
      filter_builder << " and ";
    filter_builder << def.bpf;
  }

  // Creating the filter
  auto* filter = trace_create_filter(filter_builder.str().c_str());
  if (filter == NULL) {
    cerr << "Failed to create filter: '" << filter_builder.str() << "'" << endl;
    libtrace_cleanup(trace, packet, filter);
    return;
  }

  while (trace_read_packet(trace, packet) > 0) {
    // If something goes wrong when writing packets, we need to
    // catch that error, tidy up and exit.
    if (per_packet(packet, filter) == -1) {
      libtrace_cleanup(trace, packet, filter);
      return;
    }
  }

  // Checking for any errors that might have occurred while reading the
  // input trace.
  if (trace_is_err(trace)) {
    trace_perror(trace, "Reading packets");
    libtrace_cleanup(trace, packet, filter);
    return;
  }

  libtrace_cleanup(trace, packet, filter);
}

} // namespace

CAF_MAIN()
