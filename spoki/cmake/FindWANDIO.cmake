# Try to find scamperfile headers and libraries
#
# Use this module as follows:
#
#     find_package(WANDIO [REQUIRED])
#
# Variables used by this module (they can change the default behavior and need
# to be set before calling find_package):
#
#  WANDIO_ROOT_DIR Set this variable either to an installation prefix or to
#                   the WANDIO root directory where to look for the library.
#
# Variables defined by this module:
#
#  WANDIO_FOUND        Found library and header
#  WANDIO_LIBRARY      Path to library
#  WANDIO_INCLUDE_DIR  Include path for headers
#

find_library(WANDIO_LIBRARY
  NAMES
    wandio
  HINTS
    ${WANDIO_ROOT_DIR}/lib
    ${WANDIO_ROOT_DIR}/lib/.libs
)

find_path(WANDIO_INCLUDE_DIR
  NAMES
    "wandio.h"
  HINTS
    ${WANDIO_ROOT_DIR}
    ${WANDIO_ROOT_DIR}/include
    ${WANDIO_ROOT_DIR}/lib
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  WANDIO
  DEFAULT_MSG
  WANDIO_LIBRARY
  WANDIO_INCLUDE_DIR
)

mark_as_advanced(
  WANDIO_ROOT_DIR
  WANDIO_LIBRARY
  WANDIO_INCLUDE_DIR
)
