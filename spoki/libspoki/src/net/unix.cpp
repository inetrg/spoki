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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>


#include <caf/detail/socket_guard.hpp>
#include <caf/io/network/native_socket.hpp>
#include <caf/ip_address.hpp>
#include <caf/string_algorithms.hpp>

#include "spoki/config.hpp"
#include "spoki/probe/payloads.hpp"

namespace spoki::net {

// -- networking ---------------------------------------------------------------

int connect(std::string name) {
  int sockfd;
  struct sockaddr_un addr;

  if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    std::cerr << "socket: " << caf::io::network::last_socket_error_as_string()
              << std::endl;
  }

  memset(&addr, 0, sizeof(sockaddr_un));
  addr.sun_family = AF_UNIX;
  auto max_len = sizeof(addr.sun_path) - 1;
  if (name.size() > max_len) {
    std::cerr << "name is too long, limit is " << max_len << " characters\n";
    close(sockfd);
    return -1;
  }
  strncpy(addr.sun_path, name.c_str(), name.size());

  if (connect(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_un))
      == -1) {
    std::cerr << "connect: " << caf::io::network::last_socket_error_as_string()
              << std::endl;
    close(sockfd);
    return -1;
  }

  std::cout << "connected to " << name << std::endl;
  return sockfd;
}

} // namespace spoki::net
