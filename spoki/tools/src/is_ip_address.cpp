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

#include "is_ip_address.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace spoki {

bool is_ip_address(const std::string& addr) {
  struct sockaddr_in sa;
  return inet_pton(AF_INET, addr.c_str(), &(sa.sin_addr)) != 0;
}

bool is_valid_host(const std::string& addr) {
  addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_family = AF_UNSPEC;
  addrinfo* tmp = nullptr;
  if (getaddrinfo(addr.c_str(), nullptr, &hint, &tmp) != 0)
    return false;
  return true;
}

} // namespace spoki
