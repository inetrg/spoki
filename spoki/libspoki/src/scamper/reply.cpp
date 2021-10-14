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

#include "spoki/scamper/reply.hpp"

namespace spoki::scamper {

std::string to_log_line(const reply& repl, char delimiter) {
  std::string line;
  line += std::to_string(repl.start.sec);
  line += delimiter;
  line += std::to_string(repl.start.usec);
  line += delimiter;
  line += to_string(repl.probe_method);
  line += delimiter;
  line += std::to_string(repl.userid);
  line += delimiter;
  line += repl.ping_sent;
  line += delimiter;
  line += repl.src;
  line += delimiter;
  line += repl.dst;
  line += delimiter;
  line += std::to_string(repl.sport);
  line += delimiter;
  line += std::to_string(repl.dport);
  return line;
}

} // namespace spoki::scamper
