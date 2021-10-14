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

#pragma once


namespace spoki::net {

constexpr int invalid_socket = -1;

class socket_guard {
public:
  socket_guard(int fd);
  ~socket_guard();

  int release();
  void close();

private:
  int fd_;
};

} // spoki::net
