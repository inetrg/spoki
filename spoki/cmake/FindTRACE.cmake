# Try to find libtrace headers and libraries
#
# Use this module as follows:
#
#     find_package(TRACE [REQUIRED])
#
# Variables used by this module (they can change the default behavior and need
# to be set before calling find_package):
#
#  TRACE_ROOT_DIR Set this variable either to an installation prefix or to
#                 the TRACE root directory where to look for the library.
#
# Variables defined by this module:
#
#  TRACE_FOUND        Found library and header
#  TRACE_LIBRARY      Path to library
#  TRACE_INCLUDE_DIR  Include path for headers
#

find_library(TRACE_LIBRARY
  NAMES
    trace
  HINTS 
    ${TRACE_ROOT_DIR}/lib
    ${TRACE_ROOT_DIR}/lib/.libs
)

find_path(TRACE_INCLUDE_DIR
  NAMES
    "libtrace_parallel.h"
  HINTS
    ${TRACE_ROOT_DIR}/include
    ${TRACE_ROOT_DIR}/lib/
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  TRACE
  DEFAULT_MSG
  TRACE_LIBRARY
  TRACE_INCLUDE_DIR
)

mark_as_advanced(
  TRACE_ROOT_DIR
  TRACE_LIBRARY
  TRACE_INCLUDE_DIR
)
