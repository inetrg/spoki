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

#include "spoki/hashing.hpp"

namespace spoki {

// -- trace specific hashes ----------------------------------------------------

namespace trace {

uint64_t static_hash(const libtrace_packet_t*, void*) {
  return 0;
}

uint64_t modulo_hash(uint32_t addr, size_t max) {
  return addr % max;
}

} // namespace trace
} // namespace spoki
