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

#include <chrono>

#include "spoki/atoms.hpp"
#include "spoki/cache/shard.hpp"
#include "spoki/collector.hpp"
#include "spoki/defaults.hpp"
#include "spoki/logger.hpp"
#include "spoki/probe/request.hpp"

namespace {

constexpr uint32_t tag_cnt_max = 0xffffff; // Bitmask for the lower 24 bit.

void down_handler(caf::scheduled_actor* ptr, caf::down_msg& msg) {
  auto* self
    = caf::actor_cast<caf::stateful_actor<spoki::cache::shard_state>*>(ptr);
  if (msg.source == self->state.result_handler) {
    CS_LOG_DEBUG("collector died");
    // Don't care. It's only a problem if the prober dies.
    return;
  }
  if (msg.reason != caf::exit_reason::user_shutdown)
    CS_LOG_DEBUG("a prober died unexpectedly (" << caf::to_string(
                   msg.source) << "): " << caf::to_string(msg.reason));
  self->quit();
}

} // namespace

namespace spoki::cache {

shard_state::shard_state(caf::event_based_actor* self) : self{self} {
  // nop
}

shard_state::~shard_state() {
  // nop
}

uint32_t shard_state::next_id() {
  tag_cnt = (tag_cnt + 1) & tag_cnt_max;
  return shard_id | tag_cnt;
}

void shard_state::handle_packet(const packet& pkt) {
  auto key = pkt.get_key();
  probe::request req{};
  if (pkt.carries_tcp()) {
    // -- TCP ----------------------------------------------------
    if (!enable_tcp) {
      return;
    }
    auto& proto = pkt.protocol<net::tcp>();
    if (proto.syn && !proto.ack && !proto.rst) { // SYN
      // Send SYN ACK.
      auto uid = next_id();
      req.probe_method = probe::method::tcp_synack;
      req.user_id = uid;
      req.saddr = pkt.daddr;
      req.daddr = pkt.saddr;
      auto& proto = pkt.protocol<net::tcp>();
      req.sport = proto.dport;
      req.dport = proto.sport;
      req.anum = static_cast<uint32_t>(proto.snum + proto.payload.size() + 1);
      req.num_probes = spoki::defaults::cache::num_tcp_probes;
      self->send(tcp_prober, req, key.is_scanner_like);
    } else if (!proto.syn && proto.ack) { // ACK
      auto ep = net::endpoint{pkt.saddr, proto.sport};
      if (rst_scheduled.count(ep) > 0) {
        return;
      }
      rst_scheduled.insert(ep);
      // Send RST with a bit delay.
      auto uid = next_id();
      req.probe_method = probe::method::tcp_rst;
      req.user_id = uid;
      req.saddr = pkt.daddr;
      req.daddr = pkt.saddr;
      req.sport = proto.dport;
      req.dport = proto.sport;
      req.snum = proto.anum;
      req.num_probes = spoki::defaults::cache::num_tcp_rst_probes;
      self->delayed_send(self, spoki::defaults::cache::reset_delay, req);
    } else if (proto.fin) { // FIN
      // K, done.
      // TODO: Check rst_scheduled set! (Also do this on receiving a RST.)
      self->send(tcp_collector, pkt);
      return;
    } else {
      // Not sending an additional probe, still logging it.
      self->send(tcp_collector, pkt);
      return;
    }
    // Log TCP packet and reaction.
    self->send(tcp_collector, pkt, req);
  } else if (pkt.carries_udp()) {
    // -- UDP ----------------------------------------------------
    if (!enable_udp) {
      return;
    }
    auto uid = next_id();
    req.probe_method = probe::method::udp;
    req.user_id = uid;
    req.saddr = pkt.daddr;
    req.daddr = pkt.saddr;
    auto& proto = pkt.protocol<net::udp>();
    req.sport = proto.dport;
    req.dport = proto.sport;
    req.payload = proto.payload;
    req.num_probes = spoki::defaults::cache::num_udp_probes;
    self->send(udp_prober, req, key.is_scanner_like);
    self->send(udp_collector, pkt, req);
  } else if (pkt.carries_icmp()) {
    // -- ICMP -------------------------------------------------
    if (!enable_icmp) {
      return;
    }
    auto& proto = pkt.protocol<net::icmp>();
    if (proto.type != net::icmp_type::echo_reply) {
      auto uid = next_id();
      req.probe_method = probe::method::icmp_echo;
      req.user_id = uid;
      req.saddr = pkt.daddr;
      req.daddr = pkt.saddr;
      req.num_probes = spoki::defaults::cache::num_icmp_probes;
      self->send(icmp_prober, req);
    }
    // Log ICMP packet and reaction.
    self->send(icmp_collector, pkt, req);
  }
}

const char* shard_state::name = "shard";

caf::behavior shard(caf::stateful_actor<shard_state>* self,
                    caf::actor tcp_prober, caf::actor icmp_prober,
                    caf::actor udp_prober) {
  // Assign probers.
  self->state.tcp_prober = tcp_prober;
  self->state.icmp_prober = icmp_prober;
  self->state.udp_prober = udp_prober;
  // Monitor them.
  if (tcp_prober)
    self->monitor(tcp_prober);
  if (icmp_prober)
    self->monitor(icmp_prober);
  if (udp_prober)
    self->monitor(udp_prober);
  // Initialize state.
  auto& s = self->state;
  s.enable_icmp = !caf::get_or(self->config(), "cache.disable-icmp", false);
  s.enable_tcp = !caf::get_or(self->config(), "cache.disable-tcp", false);
  s.enable_udp = !caf::get_or(self->config(), "cache.disable-udp", false);
  // TODO: Not safe, should pass ID into the constructor instead.
  auto id = static_cast<uint8_t>(self->id() & 0xff);
  s.shard_id = id << 24;
  s.tag_cnt = 0;
  // Register message handlers.
  self->set_down_handler(down_handler);
  self->set_default_handler(caf::print_and_drop);
  return {
    // New incoming events received.
    [=](const packet& pkt) { self->state.handle_packet(pkt); },
    [=](const std::vector<packet>& packets) {
      for (auto& pkt : packets) {
        self->state.handle_packet(pkt);
      }
    },
    // Forward RESET requests.
    [=](const spoki::probe::request& req) {
      if (req.probe_method == probe::method::tcp_rst) {
        auto ep = net::endpoint{req.daddr, req.dport};
        self->state.rst_scheduled.erase(ep);
        self->send(self->state.tcp_prober, req);
      }
    },
    // Configure collector for tcp data.
    [=](spoki::collect_tcp_atom, spoki::start_atom, caf::actor handler) {
      self->send(self->state.tcp_collector, done_atom_v);
      self->state.tcp_collector = handler;
    },
    // Stop collecting tcp data.
    [=](collect_tcp_atom, stop_atom) {
      self->send(self->state.tcp_collector, done_atom_v);
    },
    // Configure collector for icmp data.
    [=](collect_icmp_atom, start_atom, caf::actor handler) {
      self->send(self->state.icmp_collector, done_atom_v);
      self->state.icmp_collector = handler;
    },
    // Stop collecting icmp data.
    [=](collect_icmp_atom, stop_atom) {
      self->send(self->state.icmp_collector, done_atom_v);
    },
    // Configure collector for udp data.
    [=](collect_udp_atom, start_atom, caf::actor handler) {
      self->send(self->state.udp_collector, done_atom_v);
      self->state.udp_collector = handler;
    },
    // Stop collecting udp data.
    [=](collect_udp_atom, stop_atom) {
      self->send(self->state.udp_collector, done_atom_v);
    },
    // Shutdown.
    [=](spoki::done_atom) {
      aout(self) << "Shard got done message" << std::endl;
      self->quit();
    },
  };
}

} // namespace spoki::cache
