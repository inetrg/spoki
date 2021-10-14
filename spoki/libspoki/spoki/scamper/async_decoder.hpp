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
#if defined(CS_MACOS)
#  define CS_KQUEUE_DECODER
#elif defined(CS_LINUX)
#  define CS_POLL_DECODER
#elif defined(CS_BSD)
#  define CS_KQUEUE_DECODER
#endif

namespace spoki::scamper {

/// Decode warts format to extract scamper ping replies and send the results to
/// the collecting actor in a message:
/// (result_atom, ipv4_address, num_probes, probe_time, vector<data_point>)
class async_decoder {
public:
  // -- constructors, destructor and assignment operator -----------------------

  ~async_decoder();

  // -- memeber functions ------------------------------------------------------

  /// Write data to be decoded to the decoders buffer.
  void write(std::vector<uint8_t>& buf);

  /// Factory function to setup a new decoder. Returns an empty unique_ptr in
  /// case of an initialization error.
  static std::unique_ptr<async_decoder> make(caf::actor_system& sys,
                                             caf::actor collector);

private:
  /// Private constructor used by the facory.
  async_decoder(caf::actor_system& sys, caf::actor collector);

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
  void handle_decode_write();

  /// Called by the multiplexer to notify the docer that data can be read from
  /// the scamper warts decoder.
  void handle_deocde_read();

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
  int decode_out_fd_;

  /// Read data from the decoder.
  int decode_in_fd_;

  /// Read notifications.
  int notify_in_fd_;

  /// Write to notify the decoding loop.
  int notify_out_fd_;

  /// Track read state for decoding socket. We only need to register to
  /// write events while there is data to be decoded.
  bool decoding_;

  /// Map socket fd to the relate notification callbacks for events on the
  /// related socket.
  std::unordered_map<int, std::function<void()>> callbacks_;

  /// Write and read from scamper.
  scamper_writebuf_t* decode_wb_;
  scamper_file_filter_t* ffilter_;
  scamper_file_t* decode_in_;

  /// A scoped actor to return data to the collecting subscriber and avoid race
  /// condiditions when writing data to scamper's decode buffer.
  caf::scoped_actor self_;

  /// The actor that collects the decoded results.
  caf::actor subscriber_;

  // -- kqueue and poll specifics ----------------------------------------------

#ifdef CS_KQUEUE_DECODER
  /// Kqueue helper to add/remove events.
  /// @param sockfd    The socket the event relates to.
  /// @param filter    Filter whoch events to get (e.g., EVFILT_READ).
  /// @param operation What to do here (e.g., EV_ADD, EV_ENABLE).
  void mod(uint64_t sockfd, int16_t filter, uint16_t operation);

  /// kqueue handle for multiplexing.
  int kq_;
#else // CS_POLL_DECODER
  /// Management data for poll. Not the cleanest solution ...
  std::unordered_map<int, std::function<void(short)>> mods_;
#endif
};

} // namespace spoki::scamper
