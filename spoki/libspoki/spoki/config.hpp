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

#include "spoki/detail/build_config.hpp"

// Check OS and set an related flag.

#if defined(__APPLE__)
#  define SPOKI_MACOS
#elif defined(__linux__)
#  define SPOKI_LINUX
#elif defined(__FreeBSD__)
#  define SPOKI_BSD
#elif defined(WIN32) || defined(_WIN32)
#  define SPOKI_WINDOWS
#endif

#define SPOKI_VERSION 200
#define SPOKI_MAJOR_VERSION (SPOKI_VERSION / 10000)
#define SPOKI_MINOR_VERSION ((SPOKI_VERSION / 100) % 100)
#define SPOKI_PATCH_VERSION (SPOKI_VERSION % 100)

// This compiler-specific block defines:
// - SPOKI_DEPRECATED to annotate deprecated functions
// - SPOKI_PUSH_WARNINGS/SPOKI_POP_WARNINGS to surround "noisy" header includes
// - SPOKI_ANNOTATE_FALLTHROUGH to suppress warnings in switch/case statements
// - SPOKI_COMPILER_VERSION to retrieve the compiler version in SPOKI_VERSION
// format
// - One of the following:
//   + SPOKI_CLANG
//   + SPOKI_GCC
//   + SPOKI_MSVC

// sets SPOKI_DEPRECATED, SPOKI_ANNOTATE_FALLTHROUGH,
// SPOKI_PUSH_WARNINGS and SPOKI_POP_WARNINGS
// clang-format off
#if defined(__clang__)
#  define SPOKI_CLANG
#  define SPOKI_LIKELY(x) __builtin_expect((x), 1)
#  define SPOKI_UNLIKELY(x) __builtin_expect((x), 0)
#  define SPOKI_DEPRECATED __attribute__((deprecated))
#  define SPOKI_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#  define SPOKI_PUSH_WARNINGS                                                  \
    _Pragma("clang diagnostic push")                                           \
    _Pragma("clang diagnostic ignored \"-Wall\"")                              \
    _Pragma("clang diagnostic ignored \"-Wextra\"")                            \
    _Pragma("clang diagnostic ignored \"-Wundef\"")                            \
    _Pragma("clang diagnostic ignored \"-Wshadow\"")                           \
    _Pragma("clang diagnostic ignored \"-Wdeprecated\"")                       \
    _Pragma("clang diagnostic ignored \"-Wextra-semi\"")                       \
    _Pragma("clang diagnostic ignored \"-Wconversion\"")                       \
    _Pragma("clang diagnostic ignored \"-Wcast-align\"")                       \
    _Pragma("clang diagnostic ignored \"-Wfloat-equal\"")                      \
    _Pragma("clang diagnostic ignored \"-Wswitch-enum\"")                      \
    _Pragma("clang diagnostic ignored \"-Wweak-vtables\"")                     \
    _Pragma("clang diagnostic ignored \"-Wdocumentation\"")                    \
    _Pragma("clang diagnostic ignored \"-Wold-style-cast\"")                   \
    _Pragma("clang diagnostic ignored \"-Wsign-conversion\"")                  \
    _Pragma("clang diagnostic ignored \"-Wunused-template\"")                  \
    _Pragma("clang diagnostic ignored \"-Wshorten-64-to-32\"")                 \
    _Pragma("clang diagnostic ignored \"-Wunreachable-code\"")                 \
    _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"")                 \
    _Pragma("clang diagnostic ignored \"-Wc++14-extensions\"")                 \
    _Pragma("clang diagnostic ignored \"-Wunused-parameter\"")                 \
    _Pragma("clang diagnostic ignored \"-Wnested-anon-types\"")                \
    _Pragma("clang diagnostic ignored \"-Wreserved-id-macro\"")                \
    _Pragma("clang diagnostic ignored \"-Wconstant-conversion\"")              \
    _Pragma("clang diagnostic ignored \"-Wimplicit-fallthrough\"")             \
    _Pragma("clang diagnostic ignored \"-Wused-but-marked-unused\"")           \
    _Pragma("clang diagnostic ignored \"-Wdisabled-macro-expansion\"")
#  define SPOKI_PUSH_UNUSED_LABEL_WARNING                                      \
    _Pragma("clang diagnostic push")                                           \
    _Pragma("clang diagnostic ignored \"-Wunused-label\"")
#  define SPOKI_PUSH_NON_VIRTUAL_DTOR_WARNING                                  \
    _Pragma("clang diagnostic push")                                           \
    _Pragma("clang diagnostic ignored \"-Wnon-virtual-dtor\"")
#  define SPOKI_PUSH_DEPRECATED_WARNING                                        \
    _Pragma("clang diagnostic push")                                           \
    _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#  define SPOKI_POP_WARNINGS                                                   \
    _Pragma("clang diagnostic pop")
#  define SPOKI_ANNOTATE_FALLTHROUGH [[clang::fallthrough]]
#  define SPOKI_COMPILER_VERSION                                               \
    (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
#  define SPOKI_GCC
#  define SPOKI_LIKELY(x) __builtin_expect((x), 1)
#  define SPOKI_UNLIKELY(x) __builtin_expect((x), 0)
#  define SPOKI_DEPRECATED __attribute__((deprecated))
#  define SPOKI_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#  define SPOKI_PUSH_WARNINGS                                                  \
    _Pragma("GCC diagnostic push")                                             \
    _Pragma("GCC diagnostic ignored \"-Wshadow\"")                             \
    _Pragma("GCC diagnostic ignored \"-Wpragmas\"")                            \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"")                           \
    _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")                          \
    _Pragma("GCC diagnostic ignored \"-Wconversion\"")                         \
    _Pragma("GCC diagnostic ignored \"-Wfloat-equal\"")                        \
    _Pragma("GCC diagnostic ignored \"-Wc++14-extensions\"")
#  define SPOKI_PUSH_UNUSED_LABEL_WARNING                                      \
    _Pragma("GCC diagnostic push")                                             \
    _Pragma("GCC diagnostic ignored \"-Wunused-label\"")
#  define SPOKI_PUSH_NON_VIRTUAL_DTOR_WARNING                                  \
    _Pragma("GCC diagnostic push")                                             \
    _Pragma("GCC diagnostic ignored \"-Wnon-virtual-dtor\"")
#  define SPOKI_PUSH_DEPRECATED_WARNING                                        \
    _Pragma("GCC diagnostic push")                                             \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#  define SPOKI_POP_WARNINGS                                                   \
    _Pragma("GCC diagnostic pop")
#  if __GNUC__ >= 7
#    define SPOKI_ANNOTATE_FALLTHROUGH __attribute__((fallthrough))
#  else
#    define SPOKI_ANNOTATE_FALLTHROUGH static_cast<void>(0)
#  endif
#  define SPOKI_COMPILER_VERSION                                               \
     (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
    // disable thread_local on GCC/macOS due to heap-use-after-free bug:
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67135
#elif defined(_MSC_VER)
#  define SPOKI_MSVC
#  define SPOKI_LIKELY(x) x
#  define SPOKI_UNLIKELY(x) x
#  define SPOKI_DEPRECATED
#  define SPOKI_DEPRECATED_MSG(msg)
#  define SPOKI_PUSH_WARNINGS                                                  \
    __pragma(warning(push))
#  define SPOKI_PUSH_UNUSED_LABEL_WARNING                                      \
    __pragma(warning(push))                                                    \
    __pragma(warning(disable: 4102))
#  define SPOKI_PUSH_DEPRECATED_WARNING                                        \
    __pragma(warning(push))
#  define SPOKI_PUSH_NON_VIRTUAL_DTOR_WARNING                                  \
    __pragma(warning(push))
#  define SPOKI_POP_WARNINGS __pragma(warning(pop))
#  define SPOKI_ANNOTATE_FALLTHROUGH static_cast<void>(0)
#  define SPOKI_COMPILER_VERSION _MSC_FULL_VER
#  pragma warning( disable : 4624 )
#  pragma warning( disable : 4800 )
#  pragma warning( disable : 4503 )
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif // NOMINMAX
#else
#  define SPOKI_LIKELY(x) x
#  define SPOKI_UNLIKELY(x) x
#  define SPOKI_DEPRECATED
#  define SPOKI_PUSH_WARNINGS
#  define SPOKI_PUSH_NON_VIRTUAL_DTOR_WARNING
#  define SPOKI_PUSH_DEPRECATED_WARNING
#  define SPOKI_POP_WARNINGS
#  define SPOKI_ANNOTATE_FALLTHROUGH static_cast<void>(0)
#endif
// clang-format on

#define SPOKI_CRITICAL(error)                                                  \
  do {                                                                         \
    fprintf(stderr, "[FATAL] %s:%u: critical error: '%s'\n", __FILE__,         \
            __LINE__, error);                                                  \
    ::abort();                                                                 \
  } while (false)
