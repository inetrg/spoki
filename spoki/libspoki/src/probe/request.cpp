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

#include "spoki/probe/request.hpp"

#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "spoki/config.hpp"
#include "spoki/probe/payloads.hpp"

// For convenience.
using json = nlohmann::json;

namespace spoki::probe {

namespace {

// Ping command for probing, append IPv4 addr as string and a newline.
// -i N         Wait N seconds between probes.
// -o N         Require N replies.
// -c N         Send N probes to the target.
// -P METHOD    What to use for probes (default: icmp-echo)
constexpr auto ping_cmd = "ping ";
constexpr auto num_opt = "-c ";
constexpr auto method_opt = "-P ";
constexpr auto dport_opt = "-d ";
constexpr auto sport_opt = "-F ";
constexpr auto icmp_ipid_opt = sport_opt;
constexpr auto enable_spoofing_opt = "-O spoof ";
constexpr auto no_src_udp_opt = "-O nosrc ";
constexpr auto saddr_opt = "-S ";
constexpr auto payload_opt = "-B ";
constexpr auto ack_num_opt = "-A ";
constexpr auto seq_num_opt = "-A ";       // only for tcp-rst probes
constexpr auto probe_timeout_opt = "-W "; // set delay to way to answers
//constexpr auto probe_wait_opt = "-i ";    // set wait time between probes
constexpr auto user_id_opt = "-U ";    // add id to match replies
// constexpr auto ping_cmd = "ping -o 4 -c ";

// Default UDP payload.
constexpr auto default_udp_payload = "0a";

std::string make_icmp_echo_probe(const request& req) {
  std::string cmd;
  cmd.reserve(100);
  cmd += ping_cmd;
  cmd += num_opt;
  cmd += std::to_string(req.num_probes);
  cmd += " ";
  cmd += method_opt;
  cmd += probe_name(method::icmp_echo);
  cmd += " ";
  cmd += user_id_opt;
  cmd += req.user_id;
  cmd += " ";
  cmd += icmp_ipid_opt;
  cmd += "0 ";
  cmd += probe_timeout_opt;
  cmd += "0 ";
  cmd += enable_spoofing_opt;
  cmd += saddr_opt;
  cmd += caf::to_string(req.saddr);
  cmd += " ";
  cmd += caf::to_string(req.daddr);
  cmd += "\n";
  return cmd;
}

std::string make_udp_probe(const request& req) {
  std::string cmd;
  cmd.reserve(100);
  cmd += ping_cmd;
  cmd += num_opt;
  cmd += std::to_string(req.num_probes);
  cmd += " ";
  cmd += probe_timeout_opt;
  cmd += "0 ";
  cmd += method_opt;
  cmd += probe_name(method::udp);
  cmd += " ";
  cmd += user_id_opt;
  cmd += std::to_string(req.user_id);
  cmd += " ";
  cmd += dport_opt;
  cmd += std::to_string(req.dport);
  cmd += " ";
  cmd += sport_opt;
  cmd += std::to_string(req.sport);
  cmd += " ";
  cmd += payload_opt;
  if (!req.payload.empty())
    cmd += to_hex_string(req.payload);
  else
    cmd += default_udp_payload;
  cmd += " ";
  cmd += enable_spoofing_opt;
  cmd += no_src_udp_opt;
  cmd += saddr_opt;
  cmd += caf::to_string(req.saddr);
  cmd += " ";
  cmd += caf::to_string(req.daddr);
  cmd += "\n";
  return cmd;
}

std::string make_tcp_synack_probe(const request& req) {
  std::string cmd;
  cmd.reserve(100);
  cmd += ping_cmd;
  cmd += num_opt;
  cmd += std::to_string(req.num_probes);
  cmd += " ";
  cmd += method_opt;
  cmd += probe_name(method::tcp_synack);
  cmd += " ";
  cmd += user_id_opt;
  cmd += std::to_string(req.user_id);
  cmd += " ";
  cmd += dport_opt;
  cmd += std::to_string(req.dport);
  cmd += " ";
  cmd += sport_opt;
  cmd += std::to_string(req.sport);
  cmd += " ";
  cmd += probe_timeout_opt;
  cmd += "0 ";
  cmd += enable_spoofing_opt;
  cmd += ack_num_opt;
  cmd += std::to_string(req.anum);
  cmd += " ";
  cmd += saddr_opt;
  cmd += to_string(req.saddr);
  cmd += " ";
  cmd += to_string(req.daddr);
  cmd += "\n";
  return cmd;
}

std::string make_tcp_rst_probe(const request& req) {
  std::string cmd;
  cmd.reserve(100);
  cmd += ping_cmd;
  cmd += num_opt;
  cmd += std::to_string(req.num_probes);
  cmd += " ";
  cmd += probe_timeout_opt;
  cmd += "0 ";
  cmd += method_opt;
  cmd += probe_name(method::tcp_rst);
  cmd += " ";
  cmd += user_id_opt;
  cmd += std::to_string(req.user_id);
  cmd += " ";
  cmd += dport_opt;
  cmd += std::to_string(req.dport);
  cmd += " ";
  cmd += sport_opt;
  cmd += std::to_string(req.sport);
  cmd += " ";
  cmd += enable_spoofing_opt;
  cmd += seq_num_opt;
  cmd += std::to_string(req.snum);
  cmd += " ";
  cmd += saddr_opt;
  cmd += caf::to_string(req.saddr);
  cmd += " ";
  cmd += caf::to_string(req.daddr);
  cmd += "\n";
  return cmd;
}

} // namespace

std::string make_command(const request& req) {
  switch (req.probe_method) {
    case method::icmp_echo:
      return make_icmp_echo_probe(req);
    case method::udp:
      return make_udp_probe(req);
    case method::tcp_synack:
      return make_tcp_synack_probe(req);
    case method::tcp_rst:
      return make_tcp_rst_probe(req);
    default:
      std::cerr << "Cannot create probe string for request tyep: "
                << to_string(req.probe_method) << std::endl;
      return "";
  }
}

std::string make_tcp_synack_probe_pe(const request& req) {
  std::string cmd;
  cmd.reserve(100);
  cmd += ping_cmd;
  cmd += num_opt;
  cmd += std::to_string(req.num_probes);
  cmd += " ";
  cmd += method_opt;
  cmd += probe_name(method::tcp_synack);
  cmd += " ";
  cmd += user_id_opt;
  cmd += std::to_string(req.user_id);
  cmd += " ";
  cmd += dport_opt;
  cmd += std::to_string(req.dport);
  cmd += " ";
  cmd += sport_opt;
  cmd += std::to_string(req.sport);
  cmd += " ";
  cmd += probe_timeout_opt;
  cmd += "0 ";
  cmd += enable_spoofing_opt;
  cmd += ack_num_opt;
  cmd += std::to_string(req.anum);
  cmd += " ";
  cmd += saddr_opt;
  cmd += to_string(req.saddr);
  cmd += " ";
  cmd += to_string(req.daddr);
  cmd += "\n";
  return cmd;
}

std::string make_tcp_synack_probe_ss(const request& req) {
  std::stringstream s;
  s << ping_cmd << num_opt << req.num_probes
    << " "
    // Send TCP probes.
    << method_opt << probe_name(method::tcp_synack)
    << " "
    // Tag it.
    << user_id_opt << req.user_id
    << " "
    // Set ports.
    << dport_opt << req.dport << " " << sport_opt
    << req.sport << " " << probe_timeout_opt << 0
    << " "
    // Enable spoofing.
    << enable_spoofing_opt
    // Add acknowledge number.
    << ack_num_opt << req.anum
    << " "
    // Set addresses, spoof source.
    << saddr_opt << caf::to_string(req.saddr) << " "
    << caf::to_string(req.daddr) << "\n";
  return s.str();
}

// -- JSON ---------------------------------------------------------------------

void to_json(json& j, const request& x) {
  j = json{
    {"method", to_string(x.probe_method)},
    {"saddr", to_string(x.saddr)},
    {"daddr", to_string(x.daddr)},
    {"sport", x.sport},
    {"dport", x.dport},
    {"snum", x.snum},
    {"anum", x.anum},
    {"payload", probe::to_hex_string(x.payload)},
    {"num_probes", x.num_probes},
    {"userid", x.user_id},
  };
}

void from_json(const nlohmann::json&, request&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}


} // namespace spoki::probe
