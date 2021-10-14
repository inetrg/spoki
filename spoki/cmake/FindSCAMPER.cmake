# Try to find scamperfile headers and libraries
#
# Use this module as follows:
#
#     find_package(SCAMPER [REQUIRED])
#
# Variables used by this module (they can change the default behavior and need
# to be set before calling find_package):
#
#  SCAMPER_ROOT_DIR Set this variable either to an installation prefix or to
#                   the SCAMPER root directory where to look for the library.
#
# Variables defined by this module:
#
#  SCAMPER_FOUND        Found library and header
#  SCAMPER_LIBRARY      Path to library
#  SCAMPER_INCLUDE_DIR  Include path for headers
#

find_library(SCAMPER_LIBRARY
  NAMES
    scamperfile
  HINTS
    ${SCAMPER_ROOT_DIR}/scamper/.libs
    ${SCAMPER_ROOT_DIR}/lib
)

find_path(SCAMPER_INCLUDE_DIR
  NAMES
    "scamper_ping.h"
  HINTS
    ${SCAMPER_ROOT_DIR}
    ${SCAMPER_ROOT_DIR}/include
    ${SCAMPER_ROOT_DIR}/scamper
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  SCAMPER
  DEFAULT_MSG
  SCAMPER_LIBRARY
  SCAMPER_INCLUDE_DIR
)

mark_as_advanced(
  SCAMPER_ROOT_DIR
  SCAMPER_LIBRARY
  SCAMPER_INCLUDE_DIR
)
