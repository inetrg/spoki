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

#pragma once

#include <fstream>

#include <caf/all.hpp>

struct brs {
  std::ifstream in;
  static const char* name;
  ~brs() {
    std::cerr << "batch reader shutting down" << std::endl;
  }
};

caf::behavior batch_reader(caf::stateful_actor<brs>* self,
                           const std::string& name);

