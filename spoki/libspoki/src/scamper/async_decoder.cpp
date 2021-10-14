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

#include "spoki/scamper/async_decoder.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <iomanip>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <caf/ipv4_address.hpp>

#ifdef CS_KQUEUE_DECODER
#  include <sys/event.h>
#else // CS_POLL_DECODER
#  include <poll.h>
#endif

extern "C" {

// Newline between includes is for clang format.
#include <scamper_list.h>

#include <scamper_addr.h>
#include <scamper_file.h>

extern int fcntl_set(const int fd, const int flags);

} // extern C

#include "spoki/atoms.hpp"
#include "spoki/logger.hpp"
#include "spoki/probe/method.hpp"
#include "spoki/probe/payloads.hpp"
#include "spoki/scamper/broker.hpp"
#include "spoki/scamper/ping.hpp"
#include "spoki/scamper/reply.hpp"
#include "spoki/task.hpp"
#include "spoki/time.hpp"

namespace spoki::scamper {

namespace {

/// Convert a scamper mthod int to our protocol enum;
spoki::probe::method to_probe_method(uint8_t m) {
  switch (m) {
    case SCAMPER_PING_METHOD_ICMP_ECHO:
      return spoki::probe::method::icmp_echo;
    case SCAMPER_PING_METHOD_ICMP_TIME:
      return spoki::probe::method::icmp_time;
    case SCAMPER_PING_METHOD_TCP_SYN:
      return spoki::probe::method::tcp_syn;
    case SCAMPER_PING_METHOD_TCP_ACK:
      return spoki::probe::method::tcp_ack;
    case SCAMPER_PING_METHOD_TCP_ACK_SPORT:
      return spoki::probe::method::tcp_ack_sport;
    case SCAMPER_PING_METHOD_TCP_SYNACK:
      return spoki::probe::method::tcp_synack;
    case SCAMPER_PING_METHOD_TCP_RST:
      return spoki::probe::method::tcp_rst;
    case SCAMPER_PING_METHOD_UDP:
      return spoki::probe::method::udp;
    case SCAMPER_PING_METHOD_UDP_DPORT:
      return spoki::probe::method::udp_dport;
    default:
      // We are not using this one, but this should not happen.
      std::cerr << "ERR: scamper probing method " << m << " unknown\n";
      return spoki::probe::method::tcp_ack_sport;
  }
}

const std::array<const char*, 9> all_flags = {"v4rr",   "spoof",     "payload",
                                              "tsonly", "tsandaddr", "icmpsum",
                                              "dl",     "tbt",       "nosrc"};

} // namespace

async_decoder::async_decoder(caf::actor_system& sys, caf::actor collector)
  : done_{false},
    decode_out_fd_{-1},
    decode_in_fd_{-1},
    notify_in_fd_{-1},
    notify_out_fd_{-1},
    decoding_{false},
    decode_wb_{nullptr},
    ffilter_{nullptr},
    decode_in_{nullptr},
    self_(sys),
    subscriber_{std::move(collector)} {
  // nop
}

async_decoder::~async_decoder() {
  // This should stop the thread.
  stop();
  // Cleanup the scamper related files.
  if (ffilter_ != nullptr)
    scamper_file_filter_free(ffilter_);
  if (decode_wb_ != nullptr)
    scamper_writebuf_free(decode_wb_);
  if (decode_in_ != nullptr)
    scamper_file_close(decode_in_);
  // And out socket pairs.
  close(decode_out_fd_);
  close(decode_in_fd_);
  close(notify_in_fd_);
  close(notify_out_fd_);
}

std::unique_ptr<async_decoder> async_decoder::make(caf::actor_system& sys,
                                                   caf::actor subscriber) {
  // Pairs for creation;
  std::array<int, 2> decode_pair;
  std::array<int, 2> notify_pair;
  // Scamper config data.
  std::array<uint16_t, 1> types{{SCAMPER_FILE_OBJ_PING}};
  std::string file_type{"warts"};
  // Create socket pairs for decoding.
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, decode_pair.data()) != 0) {
    CS_LOG_ERROR("could not create decode pair");
    return nullptr;
  }
  // Open scamper file to read decoded data.;
  auto decode_in = scamper_file_openfd(decode_pair[0], nullptr, 'r',
                                       &file_type[0u]);
  if (decode_in == nullptr) {
    CS_LOG_ERROR("file openfd failed");
    return nullptr;
  }
  if (fcntl_set(decode_pair[0], O_NONBLOCK) == -1
      || fcntl_set(decode_pair[1], O_NONBLOCK) == -1) {
    CS_LOG_ERROR("fcntl set nonblocking failed");
    return nullptr;
  }
  // Create the filter to make sure we get only ping results.
  auto ffilter = scamper_file_filter_alloc(types.data(),
                                           static_cast<uint16_t>(types.size()));
  if (ffilter == nullptr) {
    CS_LOG_ERROR("file filter alloc failed");
    return nullptr;
  }
  // Create buffer to temporarily store data before decoding.
  auto decode_wb = scamper_writebuf_alloc();
  if (decode_wb == nullptr) {
    CS_LOG_ERROR("scamper writebuf alloc failed");
    return nullptr;
  }
  // Create socket pair for notification.
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, notify_pair.data()) != 0) {
    CS_LOG_ERROR("could not create notify pair");
    return nullptr;
  }
  if (fcntl_set(notify_pair[0], O_NONBLOCK) == -1
      || fcntl_set(notify_pair[1], O_NONBLOCK) == -1) {
    CS_LOG_ERROR("fcntl set nonblocking failed");
    return nullptr;
  }
  // Create decoder.
  using dec_ptr = std::unique_ptr<async_decoder>;
  auto dec = dec_ptr(new async_decoder{sys, std::move(subscriber)});
  // Assign data to our decoder.
  dec->decode_in_ = decode_in;
  dec->ffilter_ = ffilter;
  dec->decode_wb_ = decode_wb;
  dec->notify_in_fd_ = notify_pair[0];
  dec->notify_out_fd_ = notify_pair[1];
  dec->decode_in_fd_ = decode_pair[0];
  dec->decode_out_fd_ = decode_pair[1];
  // Start the thread.
  // TODO: Catch the possible system exception?
  dec->start();
  return dec;
}

void async_decoder::start() {
  // Set callbacks for socket event.
  callbacks_.clear();
  callbacks_[notify_in_fd_] = [&]() { handle_notify_read(); };
  callbacks_[decode_out_fd_] = [&]() { handle_decode_write(); };
  callbacks_[decode_in_fd_] = [&]() { handle_deocde_read(); };
  mpx_loop_ = std::thread(&async_decoder::run, this);
}

void async_decoder::write(std::vector<uint8_t>& buf) {
  // This might be called from another thread.
  anon_send(self_, std::move(buf));
  uint8_t tmp = 1;
  ::send(notify_out_fd_, &tmp, sizeof(tmp), 0);
}

void async_decoder::stop() {
  done_ = true;
  uint8_t tmp = 1;
  if (is_valid(notify_out_fd_)) {
    ::send(notify_out_fd_, &tmp, sizeof(tmp), 0);
    if (mpx_loop_.joinable())
      mpx_loop_.join();
  }
  mpx_loop_ = std::thread();
}

void async_decoder::handle_notify_read() {
  std::array<uint8_t, 16> tmp;
  auto res = ::recv(notify_in_fd_, tmp.data(), 16, 0);
  if (res < 0)
    CS_LOG_ERROR("notify error: " << strerror(errno));
  // Append new data to our buffer.
  self_->receive_while([&] { return self_->has_next_message(); })(
    [&](const std::vector<uint8_t>& buf) {
      if (scamper_writebuf_send(decode_wb_, buf.data(), buf.size()) != 0)
        CS_LOG_ERROR("scamper wb send failed: " << strerror(errno));
      if (!decoding_) {
        // mod(decode_out_fd, EVFILT_WRITE, EV_ENABLE);
        enable(decode_out_fd_, operation::write);
        decoding_ = true;
      }
    });
}

void async_decoder::handle_decode_write() {
  // std::lock_guard<std::mutex> guard(mtx);
  if (scamper_writebuf_write(decode_out_fd_, decode_wb_) != 0)
    CS_LOG_ERROR("!!! scamper wb write failed: " << strerror(errno));
  if (scamper_writebuf_gtzero(decode_wb_) == 0 && decoding_) {
    // mod(decode_out_fd, EVFILT_WRITE, EV_DISABLE);
    disable(decode_out_fd_, operation::write);
    decoding_ = false;
  }
}

void async_decoder::handle_deocde_read() {
  uint16_t type;
  void* data = nullptr;
  if (scamper_file_read(decode_in_, ffilter_, &type, &data) != 0) {
    CS_LOG_ERROR("warts decoder read failed");
    return;
  }
  if (data == nullptr)
    return;
  if (type != SCAMPER_FILE_OBJ_PING) {
    CS_LOG_DEBUG("decoded unknown result type");
    aout(self_) << " ERR: decoder got unexpected result type: " << type
                << std::endl;
    // TODO: We probably need to free the memory.
    return;
  }
  // Get the right pointer type.
  auto ptr = reinterpret_cast<scamper_ping_t*>(data);
  // Let's rebuild the warts2json output. That might make things a lot easier.
  // Addresses.
  caf::ipv4_address daddr;
  caf::ipv4_address saddr;
  std::string daddr_str = ping_dst(ptr);
  if (auto err = caf::parse(daddr_str, daddr))
    std::cerr << "could not parse ping destination to IPv4 address: "
              << daddr_str << std::endl;
  std::string saddr_str = ping_src(ptr);
  if (auto err = caf::parse(saddr_str, saddr))
    std::cerr << "could not parse ping source to IPv4 address: " << daddr_str
              << std::endl;
  // Ping method.
  auto probe_method = to_probe_method(ptr->probe_method);
  // Time, need start secs and usec
  scamper::timepoint start_time{ptr->start.tv_sec, ptr->start.tv_usec};
  // Read these directly below: number of probes, probe size, user id, ttl,
  // wait, timeout, sport,  dport
  // Convert payload to hex.
  std::string payload;
  if (ptr->probe_datalen > 0 && ptr->probe_data != nullptr) {
    std::vector<char> tmp(ptr->probe_data,
                          ptr->probe_data + ptr->probe_datalen);
    payload = probe::to_hex_string(tmp);
  }
  // Flags
  std::vector<std::string> flags;
  for (size_t i = 0; i < all_flags.size(); ++i)
    if (ptr->flags & (0x1 << i))
      flags.push_back(all_flags[i]);
  // Responses, but we should never get any ...
  std::vector<char> responses;
  // Statistics
  scamper::statistics stats{0, 0};
  for (auto i = 0; i < ptr->ping_sent; ++i) {
    auto reply = ptr->ping_replies[i];
    if (reply == nullptr)
      ++stats.loss;
    else
      ++stats.replies;
  }

  // Create record
  scamper::reply rep{
    "ping",
    0.4, // This seems to be hard coded.
    probe_method,
    saddr_str,
    daddr_str,
    start_time,
    ptr->ping_sent,
    ptr->probe_size,
    ptr->userid,
    ptr->probe_ttl,
    ptr->probe_wait,
    ptr->probe_timeout,
    ptr->probe_sport,
    ptr->probe_dport,
    payload,
    flags,
    responses,
    stats,
  };
  // aout(self_) << "[asd] packet from " << reply.saddr << " to " << reply.daddr
  // << std::endl; time_t unix_ts = ptr->start.tv_sec;
  //self_->send(subscriber_, probed_atom_v, std::move(rep));
  scamper_ping_free(ptr);
}

bool async_decoder::is_valid(int sockfd) {
  return fcntl(sockfd, F_GETFL) != -1 || errno != EBADF;
}

#ifdef CS_KQUEUE_DECODER

void async_decoder::run() {
  // Create kqueue for multiplexing.
  if ((kq_ = kqueue()) == -1) {
    CS_LOG_ERROR("failed to create kqueue");
    return;
  }
  // Setup our initial socket events;
  std::array<struct kevent, 3> initial_events;
  EV_SET(&initial_events[0], static_cast<uint64_t>(notify_in_fd_), EVFILT_READ,
         EV_ADD | EV_ENABLE, 0, 0, nullptr);
  EV_SET(&initial_events[1], static_cast<uint64_t>(decode_in_fd_), EVFILT_READ,
         EV_ADD | EV_ENABLE, 0, 0, nullptr);
  // We only want to enable this once we have data to write.
  EV_SET(&initial_events[2], static_cast<uint64_t>(decode_out_fd_),
         EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, nullptr);
  if (kevent(kq_, initial_events.data(), 3, nullptr, 0, nullptr) == -1) {
    CS_LOG_ERROR("failed to setup initial kqueue events");
    return;
  }
  // For our kqueue operation;
  std::array<struct kevent, 32> events;
  for (done_ = false; !done_;) {
    int cnt = kevent(kq_, nullptr, 0, events.data(), events.size(), nullptr);
    if (cnt == -1) {
      CS_LOG_ERROR("kqueue failed: " << strerror(errno));
      done_ = true;
      continue;
    }
    for (size_t i = 0; i < static_cast<size_t>(cnt); ++i) {
      if (events[i].flags & EV_EOF) {
        CS_LOG_ERROR("got eof");
        done_ = true;
      } else if (events[i].flags & EV_ERROR) {
        CS_LOG_ERROR("got error event with "
                     << events[i].data << ": "
                     << strerror(static_cast<int>(events[i].data)));
        done_ = true;
      } else if (callbacks_.count(static_cast<int>(events[i].ident)) > 0) {
        callbacks_[static_cast<int>(events[i].ident)]();
      } else {
        CS_LOG_ERROR("kqueue returned unexpected event");
      }
    }
  }
  // Remove all the events.
  EV_SET(&initial_events[0], static_cast<uint64_t>(notify_in_fd_), EVFILT_READ,
         EV_DELETE, 0, 0, nullptr);
  EV_SET(&initial_events[1], static_cast<uint64_t>(decode_in_fd_), EVFILT_READ,
         EV_DELETE, 0, 0, nullptr);
  EV_SET(&initial_events[2], static_cast<uint64_t>(decode_out_fd_),
         EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
  if (kevent(kq_, initial_events.data(), 3, nullptr, 0, nullptr) == -1) {
    CS_LOG_ERROR("failed to setup initial kqueue events");
    return;
  }
  if (kq_)
    close(kq_);
}

void async_decoder::enable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      mod(static_cast<uint64_t>(sockfd), EVFILT_READ, EV_ENABLE);
      break;
    case operation::write:
      mod(static_cast<uint64_t>(sockfd), EVFILT_WRITE, EV_ENABLE);
  }
}

void async_decoder::disable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      mod(static_cast<uint64_t>(sockfd), EVFILT_READ, EV_DISABLE);
      break;
    case operation::write:
      mod(static_cast<uint64_t>(sockfd), EVFILT_WRITE, EV_DISABLE);
  }
}

void async_decoder::mod(uint64_t sockfd, int16_t filter, uint16_t operation) {
  struct kevent new_event;
  EV_SET(&new_event, sockfd, filter, operation, 0, 0, nullptr);
  if (kevent(kq_, &new_event, 1, nullptr, 0, nullptr) == -1) {
    CS_LOG_ERROR("failed to add event to kqueue");
    done_ = true;
  }
}

#else // CS_POLL_DECODER

void async_decoder::run() {
  std::array<pollfd, 3> ufds;
  // read event to notify queue changes
  ufds[0].fd = notify_in_fd_;
  ufds[0].events = POLLIN;
  mods_[notify_in_fd_] = [&](short val) { ufds[0].events = val; };
  // write from queue to decoder
  ufds[1].fd = decode_out_fd_;
  ufds[1].events = 0;
  mods_[decode_out_fd_] = [&](short val) { ufds[1].events = val; };
  // read from decoder
  ufds[2].fd = decode_in_fd_;
  ufds[2].events = POLLIN;
  mods_[decode_in_fd_] = [&](short val) { ufds[2].events = val; };
  int rv = -1;
  const auto cnt = 3;
  const auto mask = POLLIN | POLLOUT;
  for (done_ = false; !done_;) {
    rv = ::poll(ufds.data(), ufds.size(), -1);
    if (rv == -1) {
      done_ = true;
      std::cerr << "poll returned " << rv << std::endl;
      continue;
    }
    for (size_t i = 0; i < cnt; ++i) {
      // There is either POLLIN or POLLOUT registered for a socket here.
      if (ufds[i].revents & mask) {
        if (callbacks_.count(static_cast<int>(ufds[i].fd)) > 0) {
          callbacks_[static_cast<int>(ufds[i].fd)]();
        } else {
          CS_LOG_ERROR("poll returned unexpected event");
        }
      }
    }
  }
  // TODO: cleanup.
  return;
}

void async_decoder::enable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      mods_[sockfd](POLLIN);
      break;
    case operation::write:
      mods_[sockfd](POLLOUT);
  }
}

void async_decoder::disable(int sockfd, operation) {
  mods_[sockfd](0);
}

#endif

} // namespace spoki::scamper
