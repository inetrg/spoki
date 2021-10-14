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

// TODO: I think I need Cmake to create a config file to get the defines right
//       and so on, with CAF currently chageing its log level implementation,
//       and me using CAF versions, i don't want to spent to much time on that
//       ... so ... no logging for now.

#include <caf/logger.hpp>

#include "spoki/config.hpp"

#define CS_LOG_COMPONENT "caf-spoki"
#define CS_LOG_IMPL(lvl, msg) CAF_LOG_IMPL(CS_LOG_COMPONENT, lvl, msg)

#if CS_LOG_LEVEL == CAF_LOG_LEVEL_QUIET || CS_LOG_LEVEL < CAF_LOG_LEVEL_TRACE
#  define CS_LOG_TRACE(_) CAF_VOID_STMT
#else
#  define CS_LOG_TRACE(output)                                                 \
    CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_TRACE, "ENTRY" << output);   \
    auto CAF_UNIFYN(caf_log_trace_guard_) = ::caf::detail::make_scope_guard(   \
      [=] { CAF_LOG_IMPL(CAF_LOG_COMPONENT, CAF_LOG_LEVEL_TRACE, "EXIT"); })
#endif // CAF_LOG_LEVEL_TRACE

#if CS_LOG_LEVEL < CAF_LOG_LEVEL_INFO
#  define CS_LOG_INFO(_) CAF_VOID_STMT
#else
#  define CS_LOG_INFO(output)                                                  \
    CAF_LOG_IMPL(CS_LOG_COMPONENT, CAF_LOG_LEVEL_INFO, output)
#endif // CAF_LOG_LEVEL_INFO

#if CS_LOG_LEVEL < CAF_LOG_LEVEL_WARNING
#  define CS_LOG_WARNING(_) CAF_VOID_STMT
#else
#  define CS_LOG_WARNING(output)                                               \
    CAF_LOG_IMPL(CS_LOG_COMPONENT, CAF_LOG_LEVEL_WARNING, output)
#endif // CAF_LOG_LEVEL_WARNING

#if CS_LOG_LEVEL < CAF_LOG_LEVEL_DEBUG
#  define CS_LOG_DEBUG(_) CAF_VOID_STMT
#else
#  define CS_LOG_DEBUG(output)                                                 \
    CAF_LOG_IMPL(CS_LOG_COMPONENT, CAF_LOG_LEVEL_DEBUG, output)
#endif // CAF_LOG_LEVEL_DEBUG

#if CS_LOG_LEVEL < CAF_LOG_LEVEL_ERROR
#  define CS_LOG_ERROR(_) CAF_VOID_STMT
#else
#  define CS_LOG_ERROR(output)                                                 \
    CAF_LOG_IMPL(CS_LOG_COMPONENT, CAF_LOG_LEVEL_ERROR, output)
#endif // CAF_LOG_LEVEL_ERROR
