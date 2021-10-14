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

#include <string>

#include <caf/allowed_unsafe_message_type.hpp>
#include <caf/optional.hpp>

extern "C" {

// Because Scamper's include files are not self-contained this ordering is
// important so separate with newlines to prevent clang-format from re-arranging
// them.
#include <scamper_addr.h>
#include <scamper_list.h>

#include <scamper_ping.h>

} // extern C

namespace spoki::scamper {

namespace detail {

/// Deleter to store scamper ping types in std::shared_ptr.
struct scamper_ping_deleter {
  void operator()(scamper_ping_t* p) const {
    if (p != nullptr)
      scamper_ping_free(p);
  }
};

} // namespace detail

/// Wrap a scamper ping in a shared ptr to avoid passing raw pointers.
using ping_result_ptr = std::shared_ptr<scamper_ping_t>;

/// Returns the destination address of a probe result as a string.
std::string ping_dst(scamper_ping_t* p);

/// Returns the source address of a probe result as a string.
std::string ping_src(scamper_ping_t* p);

} // namespace spoki::scamper

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(spoki::scamper::ping_result_ptr)
