cmake_minimum_required(VERSION 3.13.5...3.18 FATAL_ERROR)
project(SPOKI CXX)

# -- includes ------------------------------------------------------------------

include(CMakePackageConfigHelpers)
include(CheckCXXSourceCompiles)
include(FetchContent)
include(GNUInstallDirs)
include(GenerateExportHeader)

# -- version -------------------------------------------------------------------

set(SPOKI_VERSION_MAJOR 0)
set(SPOKI_VERSION_MINOR 2)
set(SPOKI_VERSION "${SPOKI_VERSION_MAJOR}.${SPOKI_VERSION_MINOR}.0")

if(CMAKE_HOST_SYSTEM_NAME MATCHES "OpenBSD")
  set(SPOKI_LIB_VERSION "${SPOKI_VERSION_MAJOR}.${SPOKI_VERSION_MINOR}"
      CACHE INTERNAL "The version string used for shared library objects")
else()
  set(SPOKI_LIB_VERSION "${SPOKI_VERSION}"
      CACHE INTERNAL "The version string used for shared library objects")
endif()

# -- override CMake defaults for internal cache entries ------------------------

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON
    CACHE INTERNAL "Write JSON compile commands database")

# -- general options -----------------------------------------------------------

option(BUILD_SHARED_LIBS "Build shared library targets" ON)

# -- incubator options that are off by default ---------------------------------

option(SPOKI_ENABLE_UTILITY_TARGETS
       "Include targets like consistency-check" OFF)
option(SPOKI_ENABLE_STANDALONE_BUILD
       "Fetch and bulid required CAF modules" OFF)

# -- incubator options that are on by default ----------------------------------

option(SPOKI_ENABLE_TESTING "Build unit test suites" ON)
option(SPOKI_ENABLE_EXAMPLES "Build small programs" ON)

# -- incubator options with non-boolean values ---------------------------------

set(SPOKI_SANITIZERS "" CACHE STRING
    "Comma separated sanitizers, e.g., 'address,undefined'")

# -- macOS-specific options ----------------------------------------------------

if(APPLE)
  set(CMAKE_MACOSX_RPATH ON CACHE INTERNAL "Use rpaths on macOS and iOS")
  #set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  #set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

  #SET(CMAKE_INSTALL_NAME_DIR @executable_path)
  #SET(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
  #SET(BUILD_RPATH "./scamper/scamper/.libs/;./libtrace/lib/.libs/")

  # -- 
  # https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling
  # use, i.e. don't skip the full RPATH for the build tree
  set(CMAKE_SKIP_BUILD_RPATH FALSE)

  # when building, don't use the install RPATH already
  # (but later on when installing)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

  # the RPATH to be used when installing
  set(CMAKE_INSTALL_RPATH "")

  # don't add the automatically determined parts of the RPATH
  # which point to directories outside the build tree to the install RPATH
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

  # -- 
  # use, i.e. don't skip the full RPATH for the build tree
  #set(CMAKE_SKIP_BUILD_RPATH FALSE)

  # when building, don't use the install RPATH already
  # (but later on when installing)
  #set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

  #set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

  # add the automatically determined parts of the RPATH
  # which point to directories outside the build tree to the install RPATH
  #set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

  # the RPATH to be used when installing, but only if it's not a system directory
  #list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
  #if("${isSystemDir}" STREQUAL "-1")
      #set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  #endif("${isSystemDir}" STREQUAL "-1")
endif()


# -- base target setup ---------------------------------------------------------

# This target propagates compiler flags, extra dependencies, etc. All other
# spoki targets pull this target in as a PRIVATE dependency. Users that embed
# Spoki into their own CMake scaffold (e.g., via FetchContent) may pass this
# target in with some properties predefined in order to force compiler flags
# or dependencies.
if(NOT TARGET spoki_internal)
  add_library(spoki_internal INTERFACE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  target_compile_options(spoki_internal INTERFACE -Wall -Wextra -pedantic
                         -ftemplate-depth=512 -ftemplate-backtrace-limit=0)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(spoki_internal INTERFACE -Wdocumentation)
  else()
    target_compile_options(spoki_internal INTERFACE
                           -Wno-missing-field-initializers)
  endif()
endif()


# -- make sure we have at least C++17 available --------------------------------

# TODO: simply set CXX_STANDARD when switching to CMake ≥ 3.9.6
if(NOT CMAKE_CROSSCOMPILING)
  try_compile(spoki_has_cxx_17
              "${CMAKE_CURRENT_BINARY_DIR}"
              "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-compiler-features.cpp")
  if(NOT spoki_has_cxx_17)
    if(MSVC)
      set(cxx_flag "/std:c++17")
    else()
      if(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
         AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5)
        set(cxx_flag "-std=c++1z")
      else()
        set(cxx_flag "-std=c++17")
      endif()
    endif()
    # Re-run compiler check.
    try_compile(spoki_has_cxx_17
                "${CMAKE_CURRENT_BINARY_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-compiler-features.cpp"
                COMPILE_DEFINITIONS "${cxx_flag}"
                OUTPUT_VARIABLE cxx_check_output)
    if(NOT spoki_has_cxx_17)
      message(FATAL_ERROR "\nFatal error: unable activate C++17 mode!\
                           \nPlease see README.md for supported compilers.\
                           \n\ntry_compile output:\n${cxx_check_output}")
    endif()
    target_compile_options(spoki_internal INTERFACE "${cxx_flag}")
  endif()
endif()

# -- export internal target (may be useful for re-using compiler flags) --------

set_target_properties(spoki_internal PROPERTIES EXPORT_NAME internal)

add_library(SPOKI::internal ALIAS spoki_internal)

install(TARGETS spoki_internal EXPORT SpokiTargets)

# -- get dependencies ----------------------------------------------------------

if(SPOKI_ENABLE_STANDALONE_BUILD)
  FetchContent_Declare(
    actor_framework
    GIT_REPOSITORY https://github.com/actor-framework/actor-framework.git
    GIT_TAG        d25d4f9
  )
  FetchContent_Populate(actor_framework)
  set(CAF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(CAF_ENABLE_IO_MODULE OFF CACHE BOOL "" FORCE)
  set(CAF_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(CAF_ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
  set(CAF_ENABLE_OPENSSL_MODULE OFF CACHE BOOL "" FORCE)
  set(CAF_SANITIZERS "${SPOKI_SANITIZERS}" CACHE STRING "" FORCE)
  add_subdirectory(${actor_framework_SOURCE_DIR} ${actor_framework_BINARY_DIR})
else()
  if(NOT TARGET CAF::core)
    if(NOT CAF_ROOT)
      set(CAF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/")
    endif()
    find_package(CAF COMPONENTS core io test REQUIRED)
    message(STATUS "Found CAF version ${CAF_VERSION}: ${CAF_DIR}")
  endif()
endif()

if(NOT SCAMPER_ROOT_DIR)
  set(SCAMPER_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")
endif()
find_package(SCAMPER)
if (SCAMPER_FOUND)
  include_directories(${SCAMPER_INCLUDE_DIR})
else()
  message(FATAL_ERROR "Could not find scamper. Please use the script "
                      "'setup.sh' to download, patch and build it.")
endif()

if(NOT WANDIO_ROOT_DIR)
  set(WANDIO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")
endif()
find_package(WANDIO)
if (WANDIO_FOUND)
  include_directories(${WANDIO_INCLUDE_DIR})
else()
  message(FATAL_ERROR "Could not find wandio. Please use the script "
                      "'setup.sh' to download and build it.")
endif() 

if(NOT TRACE_ROOT_DIR)
  set(TRACE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")
endif()
find_package(TRACE)
if (TRACE_FOUND)
  include_directories(${TRACE_INCLUDE_DIR})
else()
  message(FATAL_ERROR "Could not find libtrace. Please use the script "
                      "'setup.sh' to download and build it.")
endif()

option(CS_USE_EXTERNAL_JSON "Use an external JSON library" OFF)
set(JSON_MultipleHeaders ON CACHE INTERNAL "")
if(CS_USE_EXTERNAL_JSON)
  find_package(nlohmann_json 3.2.0 REQUIRED)
else()
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  add_subdirectory(json)
endif()

# -- set the library version for shared library targets ------------------------

if(CMAKE_HOST_SYSTEM_NAME MATCHES "OpenBSD")
  set(SPOKI_LIB_VERSION "${SPOKI_VERSION_MAJOR}.${SPOKI_VERSION_MINOR}"
      CACHE INTERNAL "The version string used for shared library objects")
else()
  set(SPOKI_LIB_VERSION "${SPOKI_VERSION}"
      CACHE INTERNAL "The version string used for shared library objects")
endif()

# -- sanity checks -------------------------------------------------------------

if(MSVC)
  message(FATAL_ERROR "Builds are currently not supported on MSVC")
endif()

# -- unit testing setup --------------------------------------------------------

if(SPOKI_ENABLE_TESTING)
  enable_testing()
  function(spoki_add_test_suites target)
    foreach(suiteName ${ARGN})
      string(REPLACE "." "/" suitePath ${suiteName})
      target_sources(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/test/${suitePath}.cpp")
      add_test(NAME ${suiteName}
               COMMAND ${target} -r300 -n -v5 -s "^${suiteName}$")
    endforeach()
  endfunction()
endif()

# -- utility targets -----------------------------------------------------------

if(SPOKI_ENABLE_UTILITY_TARGETS)
  add_executable(spoki-generate-enum-strings
                 EXCLUDE_FROM_ALL
                 cmake/spoki-generate-enum-strings.cpp)
  target_link_libraries(spoki-generate-enum-strings
                        PRIVATE
                          SPOKI::internal
                          CAF::core)
  add_custom_target(consistency-check)
  add_custom_target(update-enum-strings)
  # adds a consistency check that verifies that `cpp_file` is still valid by
  # re-generating the file and comparing it to the existing file
  function(spoki_add_enum_consistency_check hpp_file cpp_file)
    set(input "${CMAKE_CURRENT_SOURCE_DIR}/${hpp_file}")
    set(file_under_test "${CMAKE_CURRENT_SOURCE_DIR}/${cpp_file}")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/check/${cpp_file}")
    get_filename_component(output_dir "${output}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_dir}")
    add_custom_command(
      OUTPUT
        "${output}"
      COMMAND 
        spoki-generate-enum-strings
        "${input}"
        "${output}"
      DEPENDS
        spoki-generate-enum-strings
        "${input}"
    )
    get_filename_component(target_name "${input}" NAME_WE)
    add_custom_target("${target_name}"
                      COMMAND
                        "${CMAKE_COMMAND}"
                        "-Dfile_under_test=${file_under_test}"
                        "-Dgenerated_file=${output}"
                        -P "${PROJECT_SOURCE_DIR}/cmake/check-consistency.cmake"
                      DEPENDS "${output}")
    add_dependencies(consistency-check "${target_name}")
    add_custom_target("${target_name}-update"
                      COMMAND
                        spoki-generate-enum-strings
                        "${input}"
                        "${file_under_test}"
                       DEPENDS
                        spoki-generate-enum-strings
                        "${input}")
    add_dependencies(update-enum-strings "${target_name}-update")
  endfunction()
else()
  function(spoki_add_enum_consistency_check hpp_file cpp_file)
    # nop
  endfunction()
endif()

# -- utility functions ---------------------------------------------------------

function(spoki_export_and_install_lib component)
  add_library(SPOKI::${component} ALIAS libspoki_${component})
  target_include_directories(libspoki_${component} INTERFACE
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                             $<INSTALL_INTERFACE:include>)
  install(TARGETS libspoki_${component}
    EXPORT SpokiTargets
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component}
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${component}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${component})
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/spoki"
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          COMPONENT ${component}
          FILES_MATCHING PATTERN "*.hpp")
  get_target_property(lib_type libspoki_${component} TYPE)
  if(NOT lib_type STREQUAL "INTERFACE_LIBRARY")
    set_target_properties(libspoki_${component} PROPERTIES
                          EXPORT_NAME ${component}
                          SOVERSION ${SPOKI_VERSION}
                          VERSION ${SPOKI_LIB_VERSION}
                          OUTPUT_NAME spoki_${component})
    string(TOUPPER "SPOKI_${component}_EXPORT" export_macro_name)
    generate_export_header(
      libspoki_${component}
      EXPORT_MACRO_NAME ${export_macro_name}
      EXPORT_FILE_NAME "spoki/detail/${component}_export.hpp")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/spoki/detail/${component}_export.hpp"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/spoki/detail/")
  endif()
endfunction()

# -- convenience function for automating our component setup -------------------

# Usage:
# spoki_add_component(
#   foo
#   DEPENDENCIES
#     INTERFACE
#       ...
#     PUBLIC
#       ...
#     PRIVATE
#       ...
#   HEADERS
#     ...
#   SOURCES
#     ...
#   TEST_SOURCES
#     ...
#   TEST_SUITES
#     ...
# )
function(spoki_add_component name)
  set(varargs DEPENDENCIES HEADERS SOURCES TEST_SOURCES TEST_SUITES
      ENUM_CONSISTENCY_CHECKS)
  cmake_parse_arguments(SPOKI_ADD_COMPONENT "" "" "${varargs}" ${ARGN})
  if(NOT SPOKI_ADD_COMPONENT_HEADERS)
    message(FATAL_ERROR "Cannot add SPOKI component without at least one header.")
  endif()
  foreach(param DEPENDENCIES HEADERS)
    if(NOT SPOKI_ADD_COMPONENT_${param})
      message(FATAL_ERROR "spoki_add_component(): missing parameter ${param}")
    endif()
  endforeach()
  set(pub_lib_target "libspoki_${name}")
  set(obj_lib_target "libspoki_${name}_obj")
  set(tst_bin_target "spoki-${name}-test")
  if(SPOKI_ENABLE_TESTING AND SPOKI_ADD_COMPONENT_TEST_SOURCES)
    set(targets ${pub_lib_target} ${obj_lib_target} ${tst_bin_target})
    add_library(${obj_lib_target} OBJECT
                ${SPOKI_ADD_COMPONENT_HEADERS}
                ${SPOKI_ADD_COMPONENT_SOURCES})
    set_property(TARGET ${obj_lib_target} PROPERTY POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${obj_lib_target}
                          ${SPOKI_ADD_COMPONENT_DEPENDENCIES})
    add_library(${pub_lib_target}
                "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
                $<TARGET_OBJECTS:${obj_lib_target}>)
    add_executable(${tst_bin_target}
                   ${SPOKI_ADD_COMPONENT_TEST_SOURCES}
                   $<TARGET_OBJECTS:${obj_lib_target}>)
    target_link_libraries(${tst_bin_target} PRIVATE CAF::test
                          ${SPOKI_ADD_COMPONENT_DEPENDENCIES})
    target_include_directories(${tst_bin_target} PRIVATE
                               "${CMAKE_CURRENT_SOURCE_DIR}/test")
    if(SPOKI_ADD_COMPONENT_TEST_SUITES)
      spoki_add_test_suites(${tst_bin_target}
                            ${SPOKI_ADD_COMPONENT_TEST_SUITES})
    endif()
  else()
    set(targets ${pub_lib_target})
    add_library(${pub_lib_target}
                ${SPOKI_ADD_COMPONENT_HEADERS}
                ${SPOKI_ADD_COMPONENT_SOURCES})
    set_property(TARGET ${pub_lib_target} PROPERTY POSITION_INDEPENDENT_CODE ON)
  endif()
  target_link_libraries(${pub_lib_target} ${SPOKI_ADD_COMPONENT_DEPENDENCIES})
  foreach(target ${targets})
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 17)
    target_compile_definitions(${target} PRIVATE "libspoki_${name}_EXPORTS")
    target_include_directories(${target} PRIVATE
                               "${CMAKE_CURRENT_SOURCE_DIR}"
                               "${CMAKE_CURRENT_BINARY_DIR}")
    if(BUILD_SHARED_LIBS)
      set_target_properties(${target} PROPERTIES
                            CXX_VISIBILITY_PRESET hidden
                            VISIBILITY_INLINES_HIDDEN ON)
    endif()
  endforeach()
  spoki_export_and_install_lib(${name})
  if(SPOKI_ADD_COMPONENT_ENUM_CONSISTENCY_CHECKS)
    foreach(enum_name ${SPOKI_ADD_COMPONENT_ENUM_CONSISTENCY_CHECKS})
      string(REPLACE "." "/" path "${enum_name}")
      spoki_add_enum_consistency_check("spoki/${path}.hpp"
                                       "src/${path}_strings.cpp")
    endforeach()
  endif()
endfunction()

# -- provide an uinstall target ------------------------------------------------

# Process cmake_uninstall.cmake.in.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)

# Add uninstall target if it does not exist yet.
if(NOT TARGET uninstall)
  add_custom_target(uninstall)
endif()

add_custom_target(spoki-uninstall)
add_custom_command(TARGET spoki-uninstall
                   PRE_BUILD
                   COMMAND "${CMAKE_COMMAND}" -P
                   "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

add_dependencies(uninstall spoki-uninstall)

# -- build all components the user asked for -----------------------------------

add_subdirectory(libspoki)
add_subdirectory(tools)

# -- add top-level compiler and linker flags that propagate to clients ---------

# Propgatate sanitizer flags to downstream targets.
if(SPOKI_SANITIZERS)
  foreach(target caf_internal libcaf_core)
    target_compile_options(${target} INTERFACE
                           -fsanitize=${SPOKI_SANITIZERS}
                           -fno-omit-frame-pointer)
    if(CMAKE_VERSION VERSION_LESS 3.13)
      target_link_libraries(${target} INTERFACE
                            -fsanitize=${SPOKI_SANITIZERS}
                            -fno-omit-frame-pointer)
    else()
      target_link_options(${target} INTERFACE
                          -fsanitize=${SPOKI_SANITIZERS}
                          -fno-omit-frame-pointer)
    endif()
  endforeach()
endif()

# -- generate and install .cmake files -----------------------------------------

export(EXPORT SpokiTargets FILE SpokiTargets.cmake NAMESPACE SPOKI::)

install(EXPORT SpokiTargets
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SPOKI" NAMESPACE SPOKI::)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SpokiConfigVersion.cmake"
  VERSION ${SPOKI_VERSION}
  COMPATIBILITY ExactVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SpokiConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SpokiConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SPOKI")

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/SpokiConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/SpokiConfigVersion.cmake"
  DESTINATION
  "${CMAKE_INSTALL_LIBDIR}/cmake/SPOKI")
