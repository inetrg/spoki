add_custom_target(all_tools)

file(GLOB_RECURSE SPOKI_TOOLS_HDRS "hdr/*.hpp")

include_directories(
  ${SPOKI_HEADERS}
  ${LIBSPOKI_INCLUDE_DIRS}
  ${CAF_INCLUDE_DIRS} 
  ${SCAMPER_INCLUDE_DIR}
  ${TRACE_INCLUDE_DIR}
  ${WANDIO_INCLUDE_DIR}
  hdr
)

function(add_tool folder name)
  add_executable(${name} ${folder}/${name}.cpp ${ARGN})
  set_property(TARGET ${name} PROPERTY CXX_STANDARD 17)
  install(FILES ${folder}/${name}.cpp DESTINATION ${CMAKE_INSTALL_DATADIR}/spoki/tools)
  add_dependencies(${name} all_tools)
endfunction()

function(add_spoki_tool folder name)
  add_tool(${folder} ${name} ${SPOKI_TOOLS_HDRS} ${ARGN})
  target_link_libraries(
    ${name}
    SPOKI::core
    # ${LDFLAGS}
    ${SCAMPER_LIBRARY}
    ${TRACE_LIBRARY}
    ${WANDIO_LIBRARY}
    CAF::core
    CAF::io
    nlohmann_json::nlohmann_json
  )
endfunction()

add_spoki_tool(src measure_stringify)
add_spoki_tool(src measure_stores)
add_spoki_tool(src measure_driver)
add_spoki_tool(src measure_unix)
add_spoki_tool(src filter)
add_spoki_tool(src measure_io)
add_spoki_tool(src udprober src/udp_processor.cpp src/batch_reader.cpp)
add_spoki_tool(src udproberd)
add_spoki_tool(src spoki src/is_ip_address.cpp)

# Some performance measurements.
add_spoki_tool(src/perf spoki-ingest src/perf/counting.cpp)
add_spoki_tool(src/perf spoki-core src/perf/counting.cpp src/perf/packet_provider.cpp)
add_spoki_tool(src/perf spoki-io src/perf/counting.cpp)
add_spoki_tool(src/perf spoki-scamper src/is_ip_address.cpp)

