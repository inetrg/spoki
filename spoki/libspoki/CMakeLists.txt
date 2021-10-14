# -- header files --------------------------------------------------------------

file(GLOB_RECURSE SPOKI_HEADERS "spoki/*.hpp")

# -- generate files ------------------------------------------------------------

# configure build_config.hpp header
configure_file("${PROJECT_SOURCE_DIR}/cmake/build_config.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/spoki/detail/build_config.hpp"
               @ONLY)

# -- add targets ---------------------------------------------------------------

spoki_add_component(
  core
  DEPENDENCIES
    PUBLIC
      CAF::core
      CAF::io
      ${SCAMPER_LIBRARY}
      ${TRACE_LIBRARY}
      ${WANDIO_LIBRARY}
      $<$<CXX_COMPILER_ID:MSVC>:ws2_32>
    PRIVATE
      SPOKI::internal
      nlohmann_json::nlohmann_json
  ENUM_CONSISTENCY_CHECKS
    analysis.classification
    net.icmp_type
    net.protocol
    net.tcp_opt
    probe.method
    scamper.ec
  HEADERS
    ${SPOKI_HEADERS}
  SOURCES
    src/analysis/classification_strings.cpp
    src/analysis/classify.cpp
    src/analysis/consistency.cpp
    src/analysis/regression.cpp
    src/buffer.cpp
    src/cache/entry.cpp
    src/cache/rotating_store.cpp
    src/cache/shard.cpp
    src/cache/store.cpp
    src/cache/timeout.cpp
    src/collector.cpp
    src/crc.cpp
    src/hashing.cpp
    src/net/five_tuple.cpp
    src/net/icmp_type_strings.cpp
    src/net/icmp.cpp
    src/net/protocol_strings.cpp
    src/net/socket_guard.cpp
    src/net/tcp_opt_strings.cpp
    src/net/tcp_opt.cpp
    src/net/tcp.cpp
    src/net/udp_hdr.cpp
    src/net/udp.cpp
    src/net/unix.cpp
    src/packet.cpp
    src/probe/method_strings.cpp
    src/probe/method.cpp
    src/probe/payloads.cpp
    src/probe/request.cpp
    src/probe/udp_prober.cpp
    src/scamper/async_decoder.cpp
    src/scamper/broker.cpp
    src/scamper/driver.cpp
    src/scamper/ec_strings.cpp
    src/scamper/manager.cpp
    src/scamper/ping.cpp
    src/scamper/reply.cpp
    src/task.cpp
    src/time.cpp
    src/trace/instance.cpp
    src/trace/processing.cpp
    src/trace/reader.cpp
    src/trace/reporting.cpp
    src/trace/state.cpp
  TEST_SOURCES
    test/core-test.cpp
  TEST_SUITES
    analysis.classify
    analysis.consistency
    analysis.regression
    cache.rotating_store
    cache.store
    collector
    consistent_hash_map
    probe.payloads
  )