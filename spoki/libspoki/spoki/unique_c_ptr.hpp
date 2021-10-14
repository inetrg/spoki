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

#include <cstdlib>
#include <memory>
#include <type_traits>

#include "spoki/detail/core_export.hpp"

namespace spoki {

/// Custom deleter to cleanup raw C pointer wrapped in a std::unique_ptr.
struct SPOKI_CORE_EXPORT c_deleter {
  template <class T>
  void operator()(T* ptr) {
    // IIRC C++ allows delete on const pointers, but free doesn't.
    std::free(const_cast<typename std::remove_const<T>::type*>(ptr));
  }
};

/// Unique ptr to a raw C pointer.
template <class T>
using unique_c_ptr = std::unique_ptr<T, c_deleter>;

} // namespace spoki
