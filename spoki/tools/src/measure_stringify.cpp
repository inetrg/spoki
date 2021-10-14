
#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>

#include <caf/ipv4_address.hpp>

#include "spoki/all.hpp"
#include "spoki/probe/request.hpp"

// SPOKI_CORE_EXPORT std::string make_tcp_synack_probe_pe(const request& req);
// SPOKI_CORE_EXPORT std::string make_tcp_synack_probe_ss(const request& req);

constexpr auto runs = size_t{800000};

long long measure_ss(spoki::probe::request req) {
  std::vector<std::string> data;
  data.reserve(runs);
  auto startTime = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < runs; ++i) {
    data.emplace_back(spoki::probe::make_tcp_synack_probe_ss(req));
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
}

long long measure_pe(spoki::probe::request req) {
  std::vector<std::string> data;
  data.reserve(runs);
  auto startTime = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < runs; ++i) {
    data.emplace_back(spoki::probe::make_tcp_synack_probe_pe(req));
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
}

void caf_main(caf::actor_system& system, const caf::actor_system_config&) {

  spoki::probe::request req;
  req.user_id = 123812;
  req.probe_method = spoki::probe::method::tcp_synack;
  req.saddr = caf::ipv4_address::from_bits(0x01020308);
  req.daddr = caf::ipv4_address::from_bits(0x02020408);
  req.sport = 1337;
  req.dport = 80;
  req.anum = 123881;
  req.num_probes = 1;

  std::cout << "pe: " << measure_pe(req) << "ms\n";
  std::cout << "ss: " << measure_ss(req) << "ms\n";
}

CAF_MAIN(caf::id_block::spoki_core, caf::io::middleman)
