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

#include "spoki/net/tcp.hpp"

#include <type_traits>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <caf/detail/socket_guard.hpp>
#include <caf/io/network/native_socket.hpp>
#include <caf/ip_address.hpp>
#include <caf/string_algorithms.hpp>

#include <nlohmann/json.hpp>

#include "spoki/config.hpp"
#include "spoki/probe/payloads.hpp"

// For convenience.
using json = nlohmann::json;

namespace spoki::net {

std::string to_string(const tcp& x) {
  std::string cmd;
  cmd += "tcp(sport ";
  cmd += std::to_string(x.sport);
  cmd += ", dport ";
  cmd += std::to_string(x.dport);
  cmd += ")";
  return cmd;
}

std::string tcp_flags_str(const tcp& x) {
  std::vector<std::string> parts;
  parts.reserve(4);
  if (x.syn)
    parts.emplace_back("syn");
  if (x.ack)
    parts.emplace_back("ack");
  if (x.rst)
    parts.emplace_back("rst");
  if (x.fin)
    parts.emplace_back("fin");
  return caf::join(std::begin(parts), std::end(parts), "-");
}

// -- networking ---------------------------------------------------------------

int connect(std::string host, uint16_t port) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  auto port_str = std::to_string(port);

  if ((rv = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &servinfo))
      != 0) {
    std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
    return 1;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      std::cerr << "socket: " << caf::io::network::last_socket_error_as_string()
                << std::endl;
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      std::cerr << "connect: "
                << caf::io::network::last_socket_error_as_string() << std::endl;
      continue;
    }

    break;
  }

  if (p == NULL) {
    std::cerr << "failed to connect\n";
    return -1;
  }

  freeaddrinfo(servinfo);
  std::cout << "connected to " << host << ":" << port << std::endl;
  return sockfd;
}

// The following code is adapted from the C++ Actor Framework.
// Code at: github.com/actor-framework/actor-framework

int read(int sock, void* buf, size_t len) {
  auto sres = ::recv(sock,
                     reinterpret_cast<caf::io::network::socket_recv_ptr>(buf),
                     len, caf::io::network::no_sigpipe_io_flag);
  if (caf::io::network::is_error(sres, true)) {
    auto err = caf::io::network::last_socket_error();
    std::cerr << "recv failed on " << sock << ": "
              << caf::io::network::socket_error_as_string(err) << std::endl;
    return err;
  } else if (sres == 0) {
    // recv returns 0  when the peer has performed an orderly shutdown
    std::cout << "peer performed orderly shutdown: " << sock << std::endl;
    return -1;
  }
  return (sres > 0) ? static_cast<size_t>(sres) : 0;
}

int write(int sock, const void* buf, size_t len) {
  auto sres = ::send(sock,
                     reinterpret_cast<caf::io::network::socket_send_ptr>(buf),
                     len, caf::io::network::no_sigpipe_io_flag);
  if (caf::io::network::is_error(sres, true)) {
    // Make sure WSAGetLastError gets called immediately on Windows.
    auto err = caf::io::network::last_socket_error();
    std::cerr << "send failed on " << sock << ": "
              << caf::io::network::socket_error_as_string(err) << std::endl;
    return err;
  }
  return (sres > 0) ? static_cast<size_t>(sres) : 0;;
}

// -- JSON ---------------------------------------------------------------------

void to_json(json& j, const tcp& x) {
  j = json{{"sport", x.sport},
           {"dport", x.dport},
           {"snum", x.snum},
           {"anum", x.anum},
           {"syn", x.syn},
           {"ack", x.ack},
           {"rst", x.rst},
           {"fin", x.fin},
           {"window_size", x.window_size},
           {"options", x.options},
           {"payload", probe::to_hex_string(x.payload)}};
}

void from_json(const json&, tcp&) {
  SPOKI_CRITICAL("JSON deserialization not implemented");
}

} // namespace spoki::net
