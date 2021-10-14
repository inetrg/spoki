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

#include <arpa/inet.h>

#include "spoki/atoms.hpp"
#include "spoki/logger.hpp"
#include "spoki/net/icmp.hpp"
#include "spoki/net/protocol.hpp"
#include "spoki/net/tcp.hpp"
#include "spoki/net/tcp_opt.hpp"
#include "spoki/net/udp.hpp"
#include "spoki/net/udp_hdr.hpp"
#include "spoki/time.hpp"
#include "spoki/trace/state.hpp"

namespace spoki::trace {

namespace {

std::unordered_map<net::tcp_opt, caf::optional<uint32_t>>
get_tcp_opts(libtrace_tcp* tcp_hdr) {
  std::unordered_map<net::tcp_opt, caf::optional<uint32_t>> opts;
  auto len = static_cast<int>(tcp_hdr->doff * 4 - sizeof(libtrace_tcp_t));
  if (len == 0)
    return opts;
  unsigned char kind, optlen;
  unsigned char* data;
  auto itr = reinterpret_cast<unsigned char*>(tcp_hdr) + sizeof(libtrace_tcp_t);
  while (trace_get_next_option(&itr, &len, &kind, &optlen, &data)) {
    switch (kind) {
      case to_value(net::tcp_opt::end_of_list):
        // End of option list.
        return opts;
      case to_value(net::tcp_opt::noop):
        // This is a noop for padding.
        continue;
      case to_value(net::tcp_opt::mss):
        // MSS
        opts[net::tcp_opt::mss] = caf::none;
        continue;
      case to_value(net::tcp_opt::window_scale):
        // Window scale
        opts[net::tcp_opt::window_scale] = caf::none;
        continue;
      case to_value(net::tcp_opt::sack_permitted):
        // Selective acknowledgement permitted
        opts[net::tcp_opt::sack_permitted] = caf::none;
        continue;
      case to_value(net::tcp_opt::sack):
        // SACK
        opts[net::tcp_opt::sack] = caf::none;
        continue;
      case to_value(net::tcp_opt::timestamp):
        // Timestamp + echo of previous timestamp
        opts[net::tcp_opt::timestamp] = caf::none;
        continue;
      default:
        // Obsolete option ... skipping it.
        opts[net::tcp_opt::other] = caf::none;
        continue;
    }
  }
  return opts;
}

// You should be sure this is an IP packet when calling this.
caf::optional<spoki::packet::data> extract_protocol(libtrace_packet_t* pkt) {
  uint8_t proto = 0;
  uint32_t remaining = 0;
  auto hdr = trace_get_transport(pkt, &proto, &remaining);
  if (hdr == nullptr)
    return caf::none;
  switch (proto) {
    case TRACE_IPPROTO_ICMP: {
      auto icmp_hdr = reinterpret_cast<libtrace_icmp*>(hdr);
      net::icmp_type type;
      from_integer(icmp_hdr->type, type);
      auto res = spoki::net::icmp{type, caf::none};
      if (type == spoki::net::icmp_type::dest_unreachable) {
        // Should carry the IP header plus 8 bytes (the UDP header) as payload.
        char* pl = reinterpret_cast<char*>(
          trace_get_payload_from_icmp(icmp_hdr, &remaining));
        // Payload at least IPv4 header (20 bytes) + UDP header (8 bytes)
        if (pl != nullptr && remaining >= 28) {
          auto pl_ip_hdr = reinterpret_cast<libtrace_ip*>(pl);
          auto pl_ip_pl_start = trace_get_payload_from_ip(pl_ip_hdr, &proto,
                                                          &remaining);
          if (proto != TRACE_IPPROTO_UDP)
            return res;
          auto pl_udp_hdr = reinterpret_cast<libtrace_udp_t*>(pl_ip_pl_start);
          spoki::net::udp_hdr hdr;
          // Get the address info to see if we sent this packet .. .
          hdr.sport = ntohs(static_cast<uint16_t>(pl_udp_hdr->source));
          hdr.dport = ntohs(static_cast<uint16_t>(pl_udp_hdr->dest));
          hdr.length = ntohs(static_cast<uint16_t>(pl_udp_hdr->len));
          hdr.chksum = ntohs(static_cast<uint16_t>(pl_udp_hdr->check));
          res.unreachable = hdr;
        }
      }
      return res;
    }
    case TRACE_IPPROTO_TCP: {
      auto tcp_hdr = reinterpret_cast<libtrace_tcp*>(hdr);
      auto pl_ptr = reinterpret_cast<char*>(
        trace_get_payload_from_tcp(tcp_hdr, &remaining));
      auto len = std::min(static_cast<size_t>(remaining),
                          trace_get_payload_length(pkt));
      auto opts = get_tcp_opts(tcp_hdr);
      std::vector<char> payload(len);
      if (pl_ptr != nullptr && len > 0)
        std::copy(pl_ptr, pl_ptr + len, std::begin(payload));
      return spoki::net::tcp{
        trace_get_source_port(pkt), trace_get_destination_port(pkt),
        ntohl(tcp_hdr->seq),        ntohl(tcp_hdr->ack_seq),
        tcp_hdr->syn != 0,          tcp_hdr->ack != 0,
        tcp_hdr->rst != 0,          tcp_hdr->fin != 0,
        ntohs(tcp_hdr->window),     std::move(opts),
        std::move(payload)};
    }
    case TRACE_IPPROTO_UDP: {
      auto udp_hdr = reinterpret_cast<libtrace_udp*>(hdr);
      auto pl_ptr = reinterpret_cast<char*>(
        trace_get_payload_from_udp(udp_hdr, &remaining));
      auto len = std::min(static_cast<size_t>(remaining),
                          trace_get_payload_length(pkt));
      std::vector<char> payload(len);
      if (pl_ptr != nullptr)
        std::copy(pl_ptr, pl_ptr + len, std::begin(payload));
      return spoki::net::udp{trace_get_source_port(pkt),
                             trace_get_destination_port(pkt),
                             std::move(payload)};
    }
    default:
      return caf::none;
  }
}

} // namespace

void local::add_packet(libtrace_packet_t* pkt) {
  uint16_t ether_type;
  uint32_t remaining;
  total_packets += 1;
  auto layer3 = trace_get_layer3(pkt, &ether_type, &remaining);
  if (layer3 == nullptr) {
    CS_LOG_DEBUG("encountered packet without IP header");
    others += 1;
    return;
  }
  switch (ether_type) {
    case 0x0800: {
      ipv4_packets += 1;
      auto ip_hdr = reinterpret_cast<libtrace_ip_t*>(layer3);
      auto ts = spoki::to_time_point(trace_get_timeval(pkt));
      auto saddr = caf::ipv4_address::from_bits(ip_hdr->ip_src.s_addr);
      // Sanity check source address.
      if (enable_filters
          && (network.contains(saddr) || filter.count(saddr) > 0))
        return;
      auto daddr = caf::ipv4_address::from_bits(ip_hdr->ip_dst.s_addr);
      // Sanity check destination address.
      if (enable_filters
          && (!network.contains(daddr) || filter.count(daddr) > 0
              || daddr.is_multicast() || daddr.is_loopback()))
        return;
      auto proto = extract_protocol(pkt);
      if (!proto)
        break;
      /*
      auto worker = router.resolve(saddr);
      if (!worker) {
        std::cerr << "no route for: " << to_string(saddr) << std::endl;
        break;
      }
      auto& bucket = packets[*worker];
      */
      if (router.empty()) {
        std::cerr << "no route for: " << to_string(saddr) << std::endl;
        break;
      }
      auto last_byte = saddr[3];
      auto idx = last_byte % router.size();
      auto worker = router[idx];
      // Send or buffer packet.
      packet pkt{
        saddr,          daddr, ntohs(static_cast<uint16_t>(ip_hdr->ip_id)),
        ip_hdr->ip_ttl, ts,    std::move(*proto)};
      if (batch_size == 1) {
        self->send(worker, std::move(pkt));
      } else {
        auto& bucket = packets[worker];
        bucket.emplace_back(std::move(pkt));
        if (bucket.size() >= batch_size) {
          self->send(worker, std::move(bucket));
          bucket.clear();
          bucket.reserve(batch_size);
        }
      }
      break;
    }
    case 0x86DD: {
      ipv6_packets += 1;
      break;
    }
    default:
      others += 1;
      break;
  }
}

void local::send_all() {
  for (auto& p : packets)
    self->send(p.first, spoki::event_atom_v, std::move(p.second));
}

void local::publish_stats(libtrace_t* trace, libtrace_thread_t* thread) {
  // TODO: Instead of using the reporting thread, each thread
  // could just send its results to the collector directly which
  // can then aggregate the data.
  // Create a new result object to pass into the reporter. Note
  // this is malloc'd memory so the reporter must free it.
  auto res = new result;
  // Populate our result structure
  res->total_packets = total_packets;
  res->ipv4_packets = ipv4_packets;
  res->ipv6_packets = ipv6_packets;
  res->others = others;
  // Publish the result
  trace_publish_result(trace, thread, 0, libtrace_generic_t{res}, RESULT_USER);
}

global::global(caf::actor_system& system, std::vector<caf::actor> s,
               caf::actor p, uint64_t i, size_t batch_size,
               std::unordered_set<caf::ipv4_address> filter)
  : sys(system),
    shards(std::move(s)),
    filter(std::move(filter)),
    parent(p),
    batch_size(batch_size),
    id(i) {
  // nop
}

caf::intrusive_ptr<local> global::make_local() {
  auto l = caf::make_counted<local>(sys, filter, batch_size);
  // spoki::hash<std::string> h;
  if (shards.empty())
    std::cerr << "no shards to add to router!" << std::endl;
  for (auto& a : shards) {
    auto actor_str = to_string(a);
    l->router.push_back(a);
    /*
    // Hopefully this helps distirbuting the load a bit better.
    for (int i = 0; i < replication_factor; ++i)
      l->router.insert(std::make_pair(h(actor_str + std::to_string(i)), a));
    */
  }
  l->enable_filters = caf::get_or(sys.config(), "enable-filters", false);
  auto subnet_from_config = [&]() -> caf::optional<caf::ipv4_subnet> {
    auto network = caf::get_or(sys.config(), "network", "127.0.0.1/32");
    std::vector<std::string> parts;
    caf::split(parts, network, "/");
    if (parts.size() != 2)
      return caf::none;
    caf::ipv4_address addr;
    auto err = caf::parse(parts[0], addr);
    if (err)
      return caf::none;
    auto len = strtol(parts[1].c_str(), nullptr, 10);
    if (len <= 0 || len > 32)
      return caf::none;
    return caf::ipv4_subnet(addr, static_cast<uint8_t>(len));
  };
  if (auto host = subnet_from_config())
    l->network = *host;
  else
    std::cerr << "WARN: network not set, might get packets not addressed to "
                 "our subnet"
              << std::endl;
  return l;
}

local::local(caf::actor_system& system,
             const std::unordered_set<caf::ipv4_address>& filter,
             size_t batch_size)
  : total_packets(0),
    ipv4_packets(0),
    ipv6_packets(0),
    others(0),
    self(system),
    batch_size(batch_size),
    filter(filter),
    enable_filters(false) {
  // nop
}

} // namespace spoki::trace
