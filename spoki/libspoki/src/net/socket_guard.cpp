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

#include "spoki/net/socket_guard.hpp"

#include <unistd.h>

namespace spoki::net {

socket_guard::socket_guard(int fd) {
  fd_ = fd;
}

socket_guard::~socket_guard() {
  close();
}

int socket_guard::release() {
  auto res = fd_;
  fd_ = invalid_socket;
  return res;
}

void socket_guard::close() {
  if (fd_ != invalid_socket) {
    ::close(fd_);
    fd_ = invalid_socket;
  }
}

} // spoki::net
