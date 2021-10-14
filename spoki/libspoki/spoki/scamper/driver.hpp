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

#include "spoki/atoms.hpp"
#include "spoki/config.hpp"
#include "spoki/detail/core_export.hpp"
#include "spoki/probe/request.hpp"

SPOKI_PUSH_WARNINGS
extern "C" {
// Because Scamper's include files are not self-contained this ordering is
// important so separate with newlines to prevent clang-format from re-arranging
// them.
#include <scamper_list.h>

#include <scamper_addr.h>
#include <scamper_file.h>
#include <scamper_writebuf.h>

#include <scamper_ping.h>

} // extern C
SPOKI_POP_WARNINGS

#include "spoki/config.hpp"
#include "spoki/operation.hpp"

// Pick a backend for the multiplexer depending on the OS, supports macOS, Linux
// and BSD.
#if defined(SPOKI_MACOS)
#  define SPOKI_KQUEUE_DECODER
#elif defined(SPOKI_LINUX)
#  define SPOKI_EPOLL_DECODER
#elif defined(SPOKI_BSD)
#  define SPOKI_KQUEUE_DECODER
#endif

namespace spoki::scamper {

/// Decode warts format to extract scamper ping replies and send the results to
/// the collecting actor in a message:
/// (result_atom, ipv4_address, num_probes, probe_time, vector<data_point>)
class SPOKI_CORE_EXPORT driver : public caf::ref_counted {
public:
  // -- constructors, destructor and assignment operator -----------------------

  driver(caf::actor_system& sys);

  ~driver();

  // -- memeber functions ------------------------------------------------------

  /// Write data to be decoded to the decoders buffer.
  void probe(const spoki::probe::request& req);

  /// Factory function to create a new driver that manages a scamper daemon on
  /// `host:port`. Returns an empty pointer on failure.
  static caf::intrusive_ptr<driver> make(caf::actor_system& sys,
                                         std::string host, uint16_t port);

  /// Factory function to create a new driver that manages a scamper daemon via
  /// a unix domain socket `name`. Returns an empty pointer on failure.
  static caf::intrusive_ptr<driver> make(caf::actor_system& sys,
                                         std::string name);

  /// Returns false if the thread is not joinable.
  bool join();

  /// Returns an ID unique to this driver (within an actor system).
  uint64_t id();

  /// More to stats and insights.
  void set_collector(caf::actor col);

private:
  /// Creates a new driver form a sockfd connected to scamper instance. Returns
  /// an empty pointer on failure.
  static caf::intrusive_ptr<driver> make(caf::actor_system& sys, int sockfd);

  /// Start the deocder multiplexer thread.
  void start();

  /// Stop the multiplexer thread.
  void stop();

  /// Called by std::thread to run the decoder.
  void run();

  /// Returns `true`  id a given `sockfd` is valid.
  bool is_valid(int sockfd);

  /// Called by the multiplexer to notify the driver of new data in its buffer.
  void handle_notify_read();

  /// Called by the multiplexer to notify the driver when new data can be
  /// written into the scamper warts decoder.
  void handle_decode_write();

  /// Called by the multiplexer to notify the driver that data can be read from
  /// the scamper warts decoder.
  void handle_deocde_read();

  /// Called by the multiplexer to notify the driver that data can be sent to
  /// scamper.
  void handle_scamper_write();

  /// Called by the multiplexer to notify that driver that new data from scamper
  /// is available.
  void handle_scamper_read();

  /// Handle data messages from scamper.
  void scamper_handle_data(const std::string_view& buf);

  /// Handle commands from scamper.
  void scamper_handle_cmd(const std::string_view& buf);

  /// Enable `op` event for `sockfd`.
  void enable(int sockfd, operation op);

  /// Disable `op` event for `sockfd`.
  void disable(int sockfd, operation op);

  /// Thread that runs the multiplexer loop.
  std::thread mpx_loop_;

  /// Tracks the working stats of the multiplexer. Setting this to true will
  /// cause the multiplexer at the next opportunity.
  bool done_;

  /// Write data into the decoder.
  int decode_write_fd_;

  /// Read data from the decoder.
  int decode_read_fd_;

  /// Write to notify the decoding loop.
  int notify_write_fd_;

  /// Read notifications.
  int notify_read_fd_;

  /// Connection to the scamper daemon.
  int scamper_fd_;

  /// Buffer to read scamper data into.
  std::vector<caf::byte> scamper_read_buf_;
  size_t scamper_bytes_in_buffer_;
  // size_t scamper_bytes_handled_;

  /// Scamper data handling.
  size_t scamper_expected_data_;

  /// Track additonal request scamper can handle.
  size_t scamper_more_;

  /// Track writing state for scamper socket.
  bool scamper_writing_;

  /// Probe requests.
  std::deque<std::string> scamper_probe_requests_;

  // Offset while writing.
  size_t scamper_written_;

  /// Track read state for decoding socket. We only need to register to
  /// write events while there is data to be decoded.
  bool decoding_;

  /// Map socket fd to the relate notification callbacks for events on the
  /// related socket.
  std::unordered_map<int, std::function<void(operation)>> callbacks_;

  /// Write and read from scamper.
  scamper_writebuf_t* decode_wb_;
  scamper_file_filter_t* ffilter_;
  scamper_file_t* decode_in_;

  /// A scoped actor to return data to the collecting subscriber and avoid race
  /// condiditions when writing data to scamper's decode buffer.
  caf::scoped_actor self_;

  /// Count send-out requests and send them out in batches to reduce messages.
  uint32_t count_batch_ = 0;

  /// The actor that collects the decoded results.
  caf::actor collector_;

  /// -- Testing Request -------------------------------------------------------
  /*
  uint32_t daddr_prefix = 0x0000010A;
  uint32_t daddr_suffix_max = 0x00feffff;
  uint32_t daddr = 1;
  uint32_t user_id_counter = 0;
  probe::request req;
  */

  // -- kqueue and poll specifics ----------------------------------------------

#if defined(SPOKI_KQUEUE_DECODER)
  /// Kqueue helper to add/remove events.
  /// @param sockfd    The socket the event relates to.
  /// @param filter    Filter whoch events to get (e.g., EVFILT_READ).
  /// @param operation What to do here (e.g., EV_ADD, EV_ENABLE).
  void mod(uint64_t sockfd, int16_t filter, uint16_t operation);

  void add(int sockfd, operation op);

  /// kqueue handle for multiplexing.
  int kq_;
#elif defined(SPOKI_POLL_DECODER)
  /// Management data for poll. Not the cleanest solution ...
  std::unordered_map<int, std::function<void(short)>> mods_;
  std::unordered_map<int, std::function<void(short)>> add_;
  std::unordered_map<int, std::function<void(short)>> del_;
#else  // defined(SPOKI_EPOLL_DECODER)
  /// Management data for epoll. Not the cleanest solution ...
  /// Kqueue helper to add/remove events.
  /// @param sockfd    The socket the event relates to.
  /// @param filter    Filter whoch events to get (e.g., EVFILT_READ).
  /// @param operation What to do here (e.g., EV_ADD, EV_ENABLE).
  void mod(int sockfd, int operation, uint32_t events);

  std::unordered_map<int, uint32_t> masks_;

  /// Socket for epoll management.
  int epollfd_;
#endif // SPOKI_KQUEUE_DECODER
};

using driver_ptr = caf::intrusive_ptr<driver>;

} // namespace spoki::scamper
