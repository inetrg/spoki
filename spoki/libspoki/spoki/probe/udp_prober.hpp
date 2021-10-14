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

#include <thread>
#include <unordered_map>
#include <vector>

#include <caf/all.hpp>

#include "spoki/config.hpp"

SPOKI_PUSH_WARNINGS
extern "C" {

#include <scamper_list.h>

#include <scamper_addr.h>
#include <scamper_file.h>
#include <scamper_writebuf.h>

#include <scamper_ping.h>

} // extern C
SPOKI_POP_WARNINGS

#include "spoki/config.hpp"
#include "spoki/detail/core_export.hpp"
#include "spoki/operation.hpp"
#include "spoki/probe/payloads.hpp"
#include "spoki/task.hpp"

// Pick a backend for the multiplexer depending on the OS, supports macOS, Linux
// and BSD.
#if defined(SPOKI_MACOS)
#  define SPOKI_KQUEUE_DECODER
#elif defined(SPOKI_LINUX)
#  define SPOKI_POLL_DECODER
#elif defined(SPOKI_BSD)
#  define SPOKI_KQUEUE_DECODER
#endif

namespace spoki::probe {

struct SPOKI_CORE_EXPORT udp_request {
  udp_request() = default;
  udp_request(caf::ipv4_address saddr, caf::ipv4_address daddr, uint16_t sport,
              uint16_t dport, std::vector<char> pl);
  udp_request(const udp_request&) = default;
  udp_request(udp_request&&) = default;
  caf::ipv4_address saddr;
  caf::ipv4_address daddr;
  uint16_t sport;
  uint16_t dport;
  std::vector<char> payload;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, udp_request& x) {
  return f.object(x).fields(f.field("saddr", x.saddr),
                            f.field("daddr", x.daddr),
                            f.field("sport", x.sport),
                            f.field("dport", x.dport),
                            f.field("payload", x.payload));
}

class SPOKI_CORE_EXPORT udp_prober : public caf::ref_counted {
public:
  using udp_prober_ptr = caf::intrusive_ptr<udp_prober>;

  // -- constructors, destructor and assignment operator -----------------------

  udp_prober(int sockfd, payload_map payloads = {});

  ~udp_prober();

  // -- factory ----------------------------------------------------------------

  static udp_prober_ptr make(bool service_specific = true,
                             bool reflect = false);

  // -- member functions -------------------------------------------------------

  /// udp_request probe to target `addr`:`port`.
  void add_request(caf::ipv4_address saddr, caf::ipv4_address daddr,
                   uint16_t sport, uint16_t dport, std::vector<char>& pl);

  /// Stop everything.
  void shutdown();

private:
  /// Start the deocder multiplexer thread.
  void start();

  /// Stop the multiplexer thread.
  void stop();

  /// Called by std::thread to run the decoder.
  void run();

  /// Returns `true`  id a given `sockfd` is valid.
  bool is_valid(int sockfd);

  /// Called by the multiplexer to notify the decoder of new data in its buffer.
  void handle_notify_read();

  /// Called by the multiplexer to notify the decoder when new data can be
  /// written into the scamper warts decoder.
  void handle_probe_write();

  /// Called by the multiplexer to notify the docer that data can be read from
  /// the scamper warts decoder.
  void handle_probe_read();

  /// Enable `op` event for `sockfd`.
  void enable(int sockfd, operation op);

  /// Disable `op` event for `sockfd`.
  void disable(int sockfd, operation op);

  // -- request handling -------------------------------------------------------

  /// Synchronization
  std::mutex mtx_;

  /// Probe udp_requests
  std::deque<udp_request> requests_;

  /// Payload
  bool reflect_;
  payload_map::mapped_type default_payload_;
  payload_map payloads_;

  // -- socket & mutliplexing state --------------------------------------------

  /// Thread that runs the multiplexer loop.
  std::thread mpx_loop_;

  /// Tracks the working stats of the multiplexer. Setting this to true will
  /// cause the multiplexer at the next opportunity.
  bool done_;

  /// Track if we got things to write.
  bool writing_;

  /// UDP socket to send probes from.
  int probe_out_fd_;

  /// Notification stuff.
  int notify_in_fd_;
  int notify_out_fd_;

  /// Map socket fd to the relate notification callbacks for events on the
  /// related socket.
  std::unordered_map<int, std::function<void()>> callbacks_;

  std::vector<char> send_buffer_;

  // -- kqueue and poll specifics ----------------------------------------------

#ifdef SPOKI_KQUEUE_DECODER
  /// Kqueue helper to add/remove events.
  /// @param sockfd    The socket the event relates to.
  /// @param filter    Filter whoch events to get (e.g., EVFILT_READ).
  /// @param operation What to do here (e.g., EV_ADD, EV_ENABLE).
  void mod(uint64_t sockfd, int16_t filter, uint16_t operation);

  /// kqueue handle for multiplexing.
  int kq_;
#else // SPOKI_POLL_DECODER
  /// Management data for poll. Not the cleanest solution ...
  std::unordered_map<int, std::function<void(short)>> mods_;
#endif
};

// UDP prober interface state.
struct SPOKI_CORE_EXPORT upi_state {
  udp_prober::udp_prober_ptr backend;
  static const char* name;
};

/// Create an actor for sending UDP probes.
SPOKI_CORE_EXPORT caf::behavior prober(caf::stateful_actor<upi_state>* self,
                                       udp_prober::udp_prober_ptr backend);

} // namespace spoki::probe
