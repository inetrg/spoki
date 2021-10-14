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

#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "udp_processor.hpp"

#include "spoki/atoms.hpp"
#include "spoki/packet.hpp"
#include "spoki/task.hpp"

namespace {

constexpr uint32_t batch_size = 5;
constexpr size_t threshold = 10;
constexpr auto interval = std::chrono::seconds(6);
constexpr auto timeout = std::chrono::seconds(20);

// Result file stuff
constexpr auto header = "address,port,protocol,paylod";
constexpr auto separator = ",";
constexpr auto file_prefix = "udprober";

constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

void down_handler(caf::scheduled_actor* ptr, caf::down_msg& msg) {
  aout(ptr) << ptr->name() << "got down message from " << to_string(msg.source)
            << " with reason " << caf::to_string(msg.reason) << std::endl;
}

void exit_handler(caf::scheduled_actor* ptr, caf::exit_msg& msg) {
  aout(ptr) << ptr->name() << "got exit message from " << to_string(msg.source)
            << " with reason " << caf::to_string(msg.reason) << std::endl;
}

std::string generate_file_name(const std::string& prefix) {
  time_t rawtime;
  tm* timeinfo;
  std::array<char, 80> ts;
  time(&rawtime);
  // TODO: This is not threadsafe.
  timeinfo = localtime(&rawtime);
  strftime(ts.data(), ts.size(), "%F.%T", timeinfo);
  std::stringstream ss;
  ss << prefix << "-" << ts.data() << ".out";
  return ss.str();
}

} // namespace

namespace up {

ups::ups(caf::event_based_actor* self) : asked{false}, self{self} {
  // nop
}

ups::~ups() {
  // nop
}

const char* ups::name = "udp_processor";

void ups::request_more() {
  if (!asked && in_progress.size() < threshold) {
    self->send(source, spoki::request_atom_v, batch_size);
    asked = true;
  }
}

void ups::new_target(endpoint& ep) {
  /*
  if (in_progress.count(ep.addr) > 0) {
    auto& p = in_addr[ep.addr];
    if (p.first == ep.port)
      return;
  }
  if (finished.count(ep) > 0)
    return;
  */
  pending[ep.daddr].push_back(ep);
  /*
  if (in_progress.count(ep.addr) == 0) {
    aout(self) << "New target: " << to_string(ep.addr)
               << " on " << ep.port << std::endl;
    spoki::udp_probe pr{ep.addr, host, ep.port, out_port};
    self->send(prober, spoki::request_atom_v, spoki::probe_type{pr});
    in_progress[ep] = clock::now();
  } else {
    aout(self) << "Already in progress: " << to_string(ep.addr)
               << " on " << ep.port << std::endl;
  }
  */
}

void ups::start_probes() {
  for (auto& elem : pending) {
    const caf::ipv4_address& addr = elem.first;
    if (in_progress.count(addr) == 0)
      start_probe(addr);
  }
}

void ups::start_probe(caf::ipv4_address addr) {
  auto& q = pending[addr];
  if (q.empty())
    return;
  auto& next = q.front();
  aout(self) << "requesting probe for " << to_string(next.daddr) << ":"
             << next.dport << std::endl;
  self->send(prober, spoki::request_atom_v, next.saddr, next.daddr,
             next.sport, next.dport, next.payload);
  in_progress[next.daddr] = std::make_pair(next, clock::now());
  q.pop_front();
}

void ups::retransmit(caf::ipv4_address addr) {
  if (in_progress.count(addr) == 0)
    return;
  auto& ep = in_progress[addr].first;
  self->send(prober, spoki::request_atom_v, ep.saddr, ep.daddr, ep.sport,
             ep.dport, ep.payload);
  aout(self) << "retransmitting probe for " << to_string(addr) << ":"
             << ep.dport << std::endl;
}

void ups::next_out_file() {
  if (out.is_open())
    out.close();
  auto filename = generate_file_name(file_prefix);
  out.open(filename);
  if (out.is_open()) {
    out << header << std::endl;
  } else {
    std::cerr << "ERR: failed to open out file: '" << filename << "'"
              << std::endl;
    self->send(self, spoki::done_atom_v);
    return;
  }
  self->delayed_send(self, std::chrono::hours(1), spoki::rotate_atom_v);
}

void ups::append(caf::ipv4_address addr, uint16_t port, caf::string_view proto,
                 const std::vector<char>& payload) {
  std::stringstream hexstr;
  // for (auto& b : payload)
  // hexstr << std::hex << std::setfill('0') << std::setw(2) << (int)b;
  /*
  hexstr << std::hex << std::setfill('0');
  auto ptr = reinterpret_cast<const unsigned char*>(payload.data());
  for (size_t i = 0; i < payload.size(); ++i, ++ptr)
    hexstr << std::setw(2) << static_cast<unsigned>(*ptr);
  */
  for (auto& b : payload) {
    hexstr << hexmap[(b & 0xF0) >> 4];
    hexstr << hexmap[(b & 0x0F) >> 0];
  }
  out << to_string(addr) << separator << port << separator << proto << separator
      << hexstr.str() << std::endl;
  out.flush();
}

caf::behavior udp_processor(caf::stateful_actor<ups>* self, caf::actor prober,
                            caf::actor source, caf::ipv4_address host) {
  self->state.source = source;
  self->state.host = host;
  self->state.prober = prober;
  self->delayed_send(self, interval, spoki::tick_atom_v);
  self->set_default_handler(caf::print_and_drop);
  self->set_down_handler(down_handler);
  self->set_exit_handler(exit_handler);
  self->state.next_out_file();
  return {
    // Probe reply received via libtrace.
    [=](spoki::event_atom, std::vector<spoki::packet>& packets) {
      // aout(self) << "Got event" << std::endl;
      auto& s = self->state;
      for (auto& pkt : packets) {
        // Is this UDP?
        if (pkt.carries_udp()) {
          auto& proto = pkt.protocol<spoki::net::udp>();
          // Do we have a pending probe to the target?
          if (s.in_progress.count(pkt.saddr) > 0) {
            auto& ep = s.in_progress[pkt.saddr].first;
            // Is it from the expected port?
            if (ep.dport == proto.sport) {
              // TODO: Record some stuff, like what is in the reply?
              aout(self) << "Got UDP reply for " << to_string(pkt.saddr) << ":"
                         << proto.sport << std::endl;
              s.append(pkt.saddr, proto.sport, "udp", proto.payload);
              s.finished[ep] = true;
              s.in_progress.erase(pkt.saddr);
              s.start_probe(pkt.saddr);
              // TODO: Try to send probes to other hosts as well?
            }
          }
        } else if (pkt.carries_icmp()) {
          // Might be a destiantion unreachable message.
          // Only works if we only send a single probe to each host at a time.
          auto& proto = pkt.protocol<spoki::net::icmp>();
          if (s.in_progress.count(pkt.saddr) > 0) {
            auto& ep = s.in_progress[pkt.saddr].first;
            if (proto.type == spoki::net::icmp_type::dest_unreachable
                && proto.unreachable && proto.unreachable->sport == ep.dport) {
              aout(self) << "Got ICMP reply from " << to_string(pkt.saddr)
                         << std::endl;
              s.append(pkt.saddr, ep.dport, "icmp");
              s.finished[ep] = false;
              s.in_progress.erase(pkt.saddr);
              s.start_probe(pkt.saddr);
            }
          }
        }
        // else: drop
      }
      s.request_more();
    },
    // New targets, in this case received from the batch_reader.
    [=](std::vector<endpoint>& targets) {
      auto& s = self->state;
      for (auto& t : targets)
        s.new_target(t);
      s.start_probes();
      // If we don't get new targets there probably aren't any left.
      if (!targets.empty())
        s.asked = false;
      s.request_more();
    },
    [=](spoki::tick_atom) {
      aout(self) << "tick" << std::endl;
      auto& s = self->state;
      auto now = clock::now();
      for (auto it = s.in_progress.begin(); it != s.in_progress.end();) {
        // Each item is a nested tuple: (addr, (port, timestamp)).
        auto diff = now - it->second.second;
        if (diff > timeout) {
          aout(self) << "No reply from " << to_string(it->first) << std::endl;
          auto& ep = it->second.first;
          s.append(ep.daddr, ep.dport, "-");
          s.finished[ep] = false;
          it = s.in_progress.erase(it);
        } else {
          if (diff > interval)
            s.retransmit(it->first);
          ++it;
        }
      }
      s.start_probes();
      s.request_more();
      self->delayed_send(self, interval, spoki::tick_atom_v);
    },
    [=](spoki::rotate_atom) { self->state.next_out_file(); },
    [=](spoki::start_atom) { self->state.request_more(); },
    [=](spoki::done_atom) { self->quit(); },
  };
}

} // namespace up
