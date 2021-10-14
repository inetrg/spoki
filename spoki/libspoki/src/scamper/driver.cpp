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

#include "spoki/scamper/driver.hpp"

#include <cassert>

#include <arpa/inet.h>
#include <fcntl.h>
#include <iomanip>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <caf/ipv4_address.hpp>

#ifdef SPOKI_KQUEUE_DECODER
#  include <sys/event.h>
#elif SPOKI_POLL_DECODER
#  include <poll.h>
#else // SPOKI_EPOLL_DECODER
#  include <sys/epoll.h>
#endif

extern "C" {

// Newline between includes is for clang format.
#include <scamper_list.h>

#include <scamper_addr.h>
#include <scamper_file.h>
#include <scamper_writebuf.h>

#include <scamper_ping.h>

extern int fcntl_set(const int fd, const int flags);

// Found in libscamperfile, but the header has a very generic name.
extern int uudecode_line(const char* in, size_t ilen, uint8_t* out,
                         size_t* olen);
extern int fcntl_set(const int fd, const int flags);

} // extern C

#include "spoki/atoms.hpp"
#include "spoki/logger.hpp"
#include "spoki/net/socket_guard.hpp"
#include "spoki/net/tcp.hpp"
#include "spoki/net/unix.hpp"
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
constexpr auto max_msg_size = size_t{512};

// Attach to a running scamper daemon.
constexpr auto attach_cmd = "attach\n";
// Detach from a running scamper daemon.
// constexpr auto done_cmd = "done\n";

#ifdef SPOKI_EPOLL_DECODER
constexpr auto epoll_errors = EPOLLRDHUP | EPOLLERR | EPOLLHUP;
#endif

} // namespace

// -- constuctor, destructor, creation -----------------------------------------

driver::driver(caf::actor_system& sys)
  : done_{false},
    decode_write_fd_{-1},
    decode_read_fd_{-1},
    notify_write_fd_{-1},
    notify_read_fd_{-1},
    scamper_bytes_in_buffer_{0},
    scamper_expected_data_{0},
    scamper_more_{1}, // The initial attach command.
    scamper_writing_{false},
    scamper_probe_requests_{attach_cmd},
    scamper_written_{0},
    decoding_{false},
    decode_wb_{nullptr},
    ffilter_{nullptr},
    decode_in_{nullptr},
    self_(sys) {
  /*
  user_id_counter = 0;
  req.probe_method = probe::method::tcp_synack;
  req.saddr = caf::ipv4_address::from_bits(0x01020308);
  req.sport = 1337;
  req.dport = 80;
  req.anum = 123881;
  req.num_probes = 1;
  */
  // collector_ = sys.spawn(counter);
}

driver::~driver() {
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
  ::close(decode_write_fd_);
  ::close(decode_read_fd_);
  ::close(notify_read_fd_);
  ::close(notify_write_fd_);
  ::close(scamper_fd_);
}

caf::intrusive_ptr<driver> driver::make(caf::actor_system& sys,
                                        std::string host, uint16_t port) {
  // Connect to scamper.
  auto scamper = net::connect(host, port);
  if (scamper == -1) {
    std::cerr << "failed to connect to scamper on: '" << host << ":" << port
              << std::endl;
    return nullptr;
  }
  caf::io::network::nonblocking(scamper, true);
  caf::io::network::tcp_nodelay(scamper, true);
  caf::io::network::allow_sigpipe(scamper, false);
  // Setup the decoding functionality, etc.
  return make(sys, scamper);
}

caf::intrusive_ptr<driver> driver::make(caf::actor_system& sys,
                                       std::string name) {
  auto scamper = net::connect(name);
  if (scamper == -1) {
    std::cerr << "failed to connect to scamper on: '" << name << "'" << std::endl;
    return nullptr;
  }
  caf::io::network::nonblocking(scamper, true);
  caf::io::network::tcp_nodelay(scamper, true);
  caf::io::network::allow_sigpipe(scamper, false);
  // Setup the decoding functionality, etc.
  return make(sys, scamper);
}


caf::intrusive_ptr<driver> driver::make(caf::actor_system& sys, int sockfd) {
  auto scamper_guard = net::socket_guard(sockfd);
  // Pairs for creation;
  std::array<int, 2> decode_pair;
  std::array<int, 2> notify_pair;
  // Scamper config data.
  std::array<uint16_t, 1> types{{SCAMPER_FILE_OBJ_PING}};
  std::string file_type{"warts"};
  // Create socket pairs for decoding.
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, decode_pair.data()) != 0) {
    std::cerr << "could not create decode pair\n";
    return nullptr;
  }
  auto decode_read_guard = net::socket_guard(decode_pair[0]);
  auto decode_write_guard = net::socket_guard(decode_pair[1]);
  // Open scamper file to read decoded data.;
  auto decode_in = scamper_file_openfd(decode_pair[0], nullptr, 'r',
                                       &file_type[0u]);
  if (decode_in == nullptr) {
    std::cerr << "file openfd failed\n";
    return nullptr;
  }
  caf::io::network::child_process_inherit(decode_pair[0], false);
  caf::io::network::child_process_inherit(decode_pair[1], false);
  caf::io::network::nonblocking(decode_pair[0], true);
  caf::io::network::nonblocking(decode_pair[1], true);
  caf::io::network::tcp_nodelay(decode_pair[0], true);
  caf::io::network::tcp_nodelay(decode_pair[1], true);
  caf::io::network::allow_sigpipe(decode_pair[0], false);
  caf::io::network::allow_sigpipe(decode_pair[1], false);
  // Create the filter to make sure we get only ping results.
  auto ffilter = scamper_file_filter_alloc(types.data(),
                                           static_cast<uint16_t>(types.size()));
  if (ffilter == nullptr) {
    std::cerr << "file filter alloc failed\n";
    scamper_file_close(decode_in);
    return nullptr;
  }
  // Create buffer to temporarily store data before decoding.
  auto decode_wb = scamper_writebuf_alloc();
  if (decode_wb == nullptr) {
    std::cerr << "scamper writebuf alloc failed\n";
    scamper_file_close(decode_in);
    scamper_file_filter_free(ffilter);
    return nullptr;
  }
  // Create socket pair for notification.
  // if (socketpair(AF_UNIX, SOCK_STREAM, 0, notify_pair.data()) != 0) {
  if (::pipe(notify_pair.data()) != 0) {
    std::cerr << "could not create notify pair\n";
    scamper_file_close(decode_in);
    scamper_writebuf_free(decode_wb);
    scamper_file_filter_free(ffilter);
    return nullptr;
  }
  // "fd[0] is set up for reading, fd[1] is set up for writing"
  auto notify_read_guard = net::socket_guard(notify_pair[0]);
  auto notify_write_guard = net::socket_guard(notify_pair[1]);
  caf::io::network::child_process_inherit(notify_pair[0], false);
  caf::io::network::child_process_inherit(notify_pair[1], false);
  caf::io::network::nonblocking(notify_pair[0], true);
  caf::io::network::tcp_nodelay(notify_pair[0], true);
  caf::io::network::allow_sigpipe(notify_pair[0], false);
  // Create decoder.
  auto drv = caf::make_counted<driver>(sys);
  // Assign data to our driver.
  drv->decode_in_ = decode_in;
  drv->ffilter_ = ffilter;
  drv->decode_wb_ = decode_wb;
  drv->notify_read_fd_ = notify_read_guard.release();
  drv->notify_write_fd_ = notify_write_guard.release();
  drv->decode_read_fd_ = decode_read_guard.release();
  drv->decode_write_fd_ = decode_write_guard.release();
  drv->scamper_fd_ = scamper_guard.release();
  // Start the thread.
  drv->start();
  return drv;
}

// -- mangement ----------------------------------------------------------------

void driver::start() {
  // Set callbacks for socket event.
  callbacks_.clear();
  callbacks_[notify_read_fd_] = [&](operation op) {
    assert(op == operation::read);
    static_cast<void>(op);
    handle_notify_read();
  };
  callbacks_[decode_write_fd_] = [&](operation op) {
    assert(op == operation::write);
    static_cast<void>(op);
    handle_decode_write();
  };
  callbacks_[decode_read_fd_] = [&](operation op) {
    assert(op == operation::read);
    static_cast<void>(op);
    handle_deocde_read();
  };
  callbacks_[scamper_fd_] = [&](operation op) {
    if (op == operation::read) {
      handle_scamper_read();
    } else {
      handle_scamper_write();
    }
  };
  mpx_loop_ = std::thread(&driver::run, this);
}

void driver::probe(const spoki::probe::request& req) {
  // This might be called from another thread.
  anon_send(self_, make_command(req));
  uint8_t tmp = 1;
  auto res = ::write(notify_write_fd_, &tmp, sizeof(tmp));
  if (res <= 0) {
    std::cerr << "notify pipe closed\n";
    abort();
  }
}

void driver::stop() {
  done_ = true;
  uint8_t tmp = 1;
  if (is_valid(notify_write_fd_)) {
    auto res = ::write(notify_write_fd_, &tmp, sizeof(tmp));
    if (res <= 0) {
      std::cerr << "notify pipe already closed\n";
    }
    if (mpx_loop_.joinable())
      mpx_loop_.join();
  }
  mpx_loop_ = std::thread();
}

bool driver::join() {
  if (mpx_loop_.joinable()) {
    mpx_loop_.join();
    return true;
  } else {
    return false;
  }
}

uint64_t driver::id() {
  return self_->id();
}

void driver::set_collector(caf::actor col) {
  collector_ = col;
}

// -- notify -------------------------------------------------------------------

void driver::handle_notify_read() {
  // aout(self_) << "handle notify read\n";
  std::array<uint8_t, 16> tmp;
  auto res = ::read(notify_read_fd_, tmp.data(), 16);
  if (res < 0)
    std::cerr << "notify error: " << strerror(errno) << std::endl;
  // Append new data to our buffer.
  self_->receive_while([&] { return self_->has_next_message(); })(
    [&](std::string& cmd) {
      // aout(self_) << "adding command to queue\n";
      scamper_probe_requests_.emplace_back(std::move(cmd));
      // aout(self_) << "[" << self_->id() << "] mores: " << scamper_more_ <<
      // std::endl;
      if (scamper_more_ > 0 && !scamper_writing_) {
        // aout(self_) << "enabling write on scamper fd\n";
        enable(scamper_fd_, operation::write);
        scamper_writing_ = true;
      }
      /*
      else {
        if (scamper_more_ == 0) {
          aout(self_) << "no pending MOREs\n";
        }
        if (scamper_writing_) {
          aout(self_) << "already writing\n";
        }
      }
      */
    });
}

// -- decode -------------------------------------------------------------------

void driver::handle_decode_write() {
  // std::lock_guard<std::mutex> guard(mtx);
  if (scamper_writebuf_write(decode_write_fd_, decode_wb_) != 0)
    std::cerr << "!!! scamper wb write failed: " << strerror(errno)
              << std::endl;
  if (scamper_writebuf_gtzero(decode_wb_) == 0 && decoding_) {
    // mod(decode_write_fd, EVFILT_WRITE, EV_DISABLE);
    disable(decode_write_fd_, operation::write);
    decoding_ = false;
  }
}

void driver::handle_deocde_read() {
  uint16_t type;
  void* data = nullptr;
  if (scamper_file_read(decode_in_, ffilter_, &type, &data) != 0) {
    std::cerr << "warts decoder read failed\n";
    return;
  }
  if (data == nullptr)
    return;
  if (type != SCAMPER_FILE_OBJ_PING) {
    // std::cerr << "decoded unknown result type\n";
    aout(self_) << " ERR: decoder got unexpected result type: " << type
                << std::endl;
    // We probably need to free the memory, but since we don't know the type?
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
  self_->send(collector_, probed_atom_v, std::move(rep));
  scamper_ping_free(ptr);
}

// -- scamper read/write -------------------------------------------------------

void driver::handle_scamper_write() {
  if (scamper_probe_requests_.empty() || scamper_more_ == 0) {
    disable(scamper_fd_, operation::write);
    scamper_writing_ = false;
    /*
    if (scamper_more_ == 0) {
      aout(self_) << "can't write because no mores are available\n";
    }
    if (scamper_probe_requests_.empty()) {
      aout(self_) << "can't write because there are no pending requests\n";
    }
    */
  }
  auto& buf = scamper_probe_requests_.front();
  int remaining = buf.size() - scamper_written_;
  // std::cout << "got " << remaining << " bytes to write\n";
  int res = 0;
  do {
    assert(!buf.empty());
    // auto view = std::string_view{buf.data() +
    // scamper_written_, static_cast<size_t>(remaining)};
    // std::cout << "writing '" << view << "' to " << scamper_fd_ << "\n";
    auto res = net::write(scamper_fd_, buf.data() + scamper_written_,
                          remaining);
    if (res < 0) {
      std::cerr << "write error\n";
      return;
    } else {
      remaining -= res;
      if (remaining == 0) {
        scamper_more_ -= 1;
        // aout(self_) << "[" << self_->id() << "] mores: " << scamper_more_ <<
        // std::endl;
        // Reset write count.
        scamper_probe_requests_.pop_front();
        scamper_written_ = 0;
        // aout(self_) << "sent out complete request\n";
        count_batch_ += 1;
        if (count_batch_ >= 100) {
          self_->send(collector_, count_batch_, scamper_probe_requests_.size());
          count_batch_ = 0;
        }
        // Check if we stop or try to write more.
        if (scamper_more_ == 0 || scamper_probe_requests_.empty()) {
          disable(scamper_fd_, operation::write);
          scamper_writing_ = false;
          return;
        }
      }
    }
  } while (res > 0 && res == remaining);
}

void driver::handle_scamper_read() {
  // std::cout << "got read event for scamper\n";
  // std::cout << "still got " << scamper_bytes_in_buffer_
  // << " bytes in the buffer\n";
  scamper_read_buf_.resize(max_msg_size);
  int res = net::read(scamper_fd_,
                      scamper_read_buf_.data() + scamper_bytes_in_buffer_,
                      scamper_read_buf_.size() - scamper_bytes_in_buffer_);
  if (res < 0) {
    std::cerr << "scamper read failed\n";
    return;
  }
  if (res == 0) {
    std::cerr << "disconnected\n";
    return;
  } else {
    scamper_bytes_in_buffer_ += res;
    scamper_read_buf_.resize(scamper_bytes_in_buffer_);
    auto data_view
      = caf::make_span(reinterpret_cast<char*>(scamper_read_buf_.data()),
                       scamper_read_buf_.size());
    auto from = data_view.begin();
    auto to = std::find(data_view.begin(), data_view.end(), '\n');
    while (to != data_view.end()) {
      auto len = std::distance(from, to);
      assert(static_cast<char>(*(from + len)) == '\n');
      assert(static_cast<char>(*(from + (len - 1))) != '\r');
      *to = '\0';
      std::string_view reply{from, static_cast<size_t>(len)};
      if (scamper_expected_data_ > 0) {
        scamper_handle_data(reply);
      } else {
        scamper_handle_cmd(reply);
      }
      from = to + 1;
      to = std::find(from, data_view.end(), '\n');
    }
    if (from == data_view.end()) {
      scamper_bytes_in_buffer_ = 0;
    } else {
      // std::cout << "erasing bytes\n";
      auto len = std::distance(data_view.begin(), from);
      scamper_read_buf_.erase(scamper_read_buf_.begin(),
                              scamper_read_buf_.begin() + len);
      scamper_bytes_in_buffer_ -= len;
    }
  }
}

// -- scamper logic ------------------------------------------------------------

void driver::scamper_handle_data(const std::string_view& buf) {
  assert(scamper_expected_data_ > 0);
  std::vector<uint8_t> uu(64);
  auto len = uu.size();
  if (uudecode_line(buf.data(), buf.size(), uu.data(), &len) != 0) {
    std::cerr << "could not uudecode data: '" << buf << "'\n";
  } else {
    if (len != 0) {
      uu.resize(len);
      if (scamper_writebuf_send(decode_wb_, uu.data(), uu.size()) != 0)
        std::cerr << "scamper wb send failed: " << strerror(errno) << std::endl;
      if (!decoding_) {
        enable(decode_write_fd_, operation::write);
        decoding_ = true;
      }
    }
    // We replaced a newline with a 0 byte, hence + 1.
    scamper_expected_data_ -= (buf.size() + 1);
  }
}

void driver::scamper_handle_cmd(const std::string_view& buf) {
  const char* head = buf.data();
  // auto line_len = buf.size();
  // Skip empty lines.
  if (head[0] == '\0') {
    return;
  }
  // Since scamper only knows these four commands we assume that a matching
  // first character uniquely identifies the command. It would probably be
  // better to actually check the data because, who knows, there might be an
  // error somewhere.
  //
  // Feedback letting us know that the command was accepted.
  // else if (line_len >= 2 && strncasecmp(head, attach_mode_ok, 2) == 0) {
  else if (head[0] == 'O') {
    // aout(self_) << "scamper: OK\n";
    return;
  }
  // If the scamper process is asking for more tasks, give it more.
  // else if (line_len == 4 && strncasecmp(head, attach_mode_more, line_len) ==
  else if (head[0] == 'M') {
    // aout(self_) << "scamper: MORE\n";
    ++scamper_more_;
    // aout(self_) << "[" << self_->id() << "] mores: " << scamper_more_ <<
    // std::endl;
    // Build scamper request.
    /*
    req.user_id = ++user_id_counter;
    daddr = (daddr + 1) & daddr_suffix_max;
    auto complete_daddr = (daddr << 8) | daddr_prefix;
    req.daddr = caf::ipv4_address::from_bits(complete_daddr);
    auto cmd = make_command(req);
    scamper_probe_requests_.emplace_back(std::move(cmd));
    */
    if (!scamper_probe_requests_.empty() && !scamper_writing_) {
      // aout(self_) << "enabling write on scamper fd\n";
      enable(scamper_fd_, operation::write);
      scamper_writing_ = true;
    }
    /*
    else {
      if (scamper_probe_requests_.empty()) {
        aout(self_) << "MORE: no requests to send\n";
      }
      if (scamper_writing_) {
        aout(self_) << "MORE: already writing\n";
      }
    }
    */
    return;
  }
  // New piece of data.
  // else if (line_len > 5 && strncasecmp(head, attach_mode_data, 5) == 0) {
  else if (head[0] == 'D') {
    auto res = strtoul(head + 5, nullptr, 10);
    if (res != 0) {
      // We'll only accept commands if there is no more data expected. Meaning,
      // we can simply overwrite the existing value.
      scamper_expected_data_ += res;
    }
    // aout(self_) << "scamper: DATA " << res << "\n";
    return;
  }
  // Feedback letting us know that the command was not accepted.
  // else if (line_len >= 3 && strncasecmp(head, attach_mode_err, 3) == 0) {
  else if (head[0] == 'E') {
    // std::cout << "scamper: ERR\n";
    std::cerr << std::string(buf.data(), buf.size()) << std::endl;
    return;
  } else {
    std::cerr << "received unknown command from scamper\n";
    return;
  }
}

// -- multiplexing -------------------------------------------------------------

bool driver::is_valid(int sockfd) {
  return fcntl(sockfd, F_GETFL) != -1 || errno != EBADF;
}

#if defined(SPOKI_KQUEUE_DECODER) // -- kqueue ---------------------------------

void driver::run() {
  aout(self_) << "running kqueue\n";
  // Create kqueue for multiplexing.
  if ((kq_ = kqueue()) == -1) {
    std::cerr << "failed to create kqueue\n";
    return;
  }
  // Setup our initial socket events;
  std::array<struct kevent, 5> initial_events;
  EV_SET(&initial_events[0], static_cast<uint64_t>(notify_read_fd_),
         EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
  EV_SET(&initial_events[1], static_cast<uint64_t>(decode_read_fd_),
         EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
  // We only want to enable this once we have data to write.
  EV_SET(&initial_events[2], static_cast<uint64_t>(decode_write_fd_),
         EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, nullptr);
  // Start reading, ...
  EV_SET(&initial_events[3], static_cast<uint64_t>(scamper_fd_), EVFILT_READ,
         EV_ADD | EV_ENABLE, 0, 0, nullptr);
  // ... writing, the protocl stats with an "attach\n" from us.
  EV_SET(&initial_events[4], static_cast<uint64_t>(scamper_fd_), EVFILT_WRITE,
         EV_ADD, 0, 0, nullptr);
  if (kevent(kq_, initial_events.data(), initial_events.size(), nullptr, 0,
             nullptr)
      == -1) {
    std::cerr << "failed to setup initial kqueue events\n";
    return;
  }
  // For our kqueue operation;
  std::array<struct kevent, 32> events;
  for (done_ = false; !done_;) {
    int cnt = kevent(kq_, nullptr, 0, events.data(), events.size(), nullptr);
    if (cnt == -1) {
      std::cerr << "kqueue failed: " << strerror(errno) << std::endl;
      done_ = true;
      continue;
    }
    for (size_t i = 0; i < static_cast<size_t>(cnt); ++i) {
      if (events[i].flags & EV_EOF) {
        std::cerr << "got eof\n";
        done_ = true;
      } else if (events[i].flags & EV_ERROR) {
        std::cerr << "got error event with " << events[i].data << ": "
                  << strerror(static_cast<int>(events[i].data)) << std::endl;
        done_ = true;
      } else if (events[i].filter == EVFILT_READ) {
        assert(callbacks_.count(static_cast<int>(events[i].ident)) > 0);
        callbacks_[static_cast<int>(events[i].ident)](operation::read);
      } else if (events[i].filter == EVFILT_WRITE) {
        assert(callbacks_.count(static_cast<int>(events[i].ident)) > 0);
        callbacks_[static_cast<int>(events[i].ident)](operation::write);
      } else {
        std::cerr << "kqueue returned unexpected event\n";
      }
    }
  }
  // Remove all the events.
  EV_SET(&initial_events[0], static_cast<uint64_t>(notify_read_fd_),
         EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  EV_SET(&initial_events[1], static_cast<uint64_t>(decode_read_fd_),
         EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  EV_SET(&initial_events[2], static_cast<uint64_t>(decode_write_fd_),
         EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
  EV_SET(&initial_events[3], static_cast<uint64_t>(decode_write_fd_),
         EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
  EV_SET(&initial_events[4], static_cast<uint64_t>(decode_write_fd_),
         EVFILT_READ, EV_DELETE, 0, 0, nullptr);
  if (kevent(kq_, initial_events.data(), initial_events.size(), nullptr, 0,
             nullptr)
      == -1) {
    std::cerr << "failed to setup initial kqueue events\n";
    return;
  }
  if (kq_)
    close(kq_);
}

void driver::add(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      mod(static_cast<uint64_t>(sockfd), EVFILT_READ, EV_ADD);
      break;
    case operation::write:
      mod(static_cast<uint64_t>(sockfd), EVFILT_WRITE, EV_ADD);
  }
}

void driver::enable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      mod(static_cast<uint64_t>(sockfd), EVFILT_READ, EV_ENABLE);
      break;
    case operation::write:
      mod(static_cast<uint64_t>(sockfd), EVFILT_WRITE, EV_ENABLE);
  }
}

void driver::disable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      mod(static_cast<uint64_t>(sockfd), EVFILT_READ, EV_DISABLE);
      break;
    case operation::write:
      mod(static_cast<uint64_t>(sockfd), EVFILT_WRITE, EV_DISABLE);
  }
}

void driver::mod(uint64_t sockfd, int16_t filter, uint16_t operation) {
  struct kevent new_event;
  EV_SET(&new_event, sockfd, filter, operation, 0, 0, nullptr);
  if (kevent(kq_, &new_event, 1, nullptr, 0, nullptr) == -1) {
    std::cerr << "failed to add event to kqueue: " << errno << std::endl;
    done_ = true;
  }
}

#elif defined(SPOKI_POLL_DECODER) // -- poll -----------------------------------

void driver::run() {
  aout(self_) << "running poll\n";
  std::array<pollfd, 4> ufds;
  // auto setup = [&ufds, this](size_t pos, int sockfd, short inital) {
  //  ufds[pos].fd = sockfd;
  //  ufds[pos].events = inital;
  //  mods_[sockfd] = [&](short val) { ufds[pos].events = val; };
  //  add_[sockfd] = [&](short val) { ufds[pos].events |= val; };
  //  del_[sockfd] = [&](short val) { ufds[pos].events &= ~val; };
  //};
  // Read event to notify queue changes, need to read these.
  // setup(0, notify_read_fd_, POLLIN);
  ufds[0].fd = notify_read_fd_;
  ufds[0].events = POLLIN;
  mods_[notify_read_fd_] = [&](short val) { ufds[0].events = val; };
  add_[notify_read_fd_] = [&](short val) { ufds[0].events |= val; };
  del_[notify_read_fd_] = [&](short val) { ufds[0].events &= ~val; };
  // Write from queue to decoder, not registered.
  // setup(1, decode_write_fd_, 0);
  ufds[1].fd = decode_write_fd_;
  ufds[1].events = 0;
  mods_[decode_write_fd_] = [&](short val) { ufds[1].events = val; };
  add_[decode_write_fd_] = [&](short val) { ufds[1].events |= val; };
  del_[decode_write_fd_] = [&](short val) { ufds[1].events &= ~val; };
  // Read from decoder, read these.
  // setup(2, decode_read_fd_, POLLIN);
  ufds[2].fd = decode_read_fd_;
  ufds[2].events = POLLIN;
  mods_[decode_read_fd_] = [&](short val) { ufds[2].events = val; };
  add_[decode_read_fd_] = [&](short val) { ufds[2].events |= val; };
  del_[decode_read_fd_] = [&](short val) { ufds[2].events &= ~val; };
  // Read from scamper, read these.
  // setup(3, scamper_fd_, POLLIN | POLLOUT);
  ufds[3].fd = scamper_fd_;
  ufds[3].events = POLLIN | POLLOUT;
  mods_[scamper_fd_] = [&](short val) { ufds[3].events = val; };
  add_[scamper_fd_] = [&](short val) { ufds[3].events |= val; };
  del_[scamper_fd_] = [&](short val) { ufds[3].events &= ~val; };
  int rv = -1;
  for (done_ = false; !done_;) {
    rv = ::poll(ufds.data(), ufds.size(), -1);
    if (rv == -1) {
      done_ = true;
      std::cerr << "poll returned " << rv << std::endl;
    } else {
      for (size_t i = 0; i < ufds.size(); ++i) {
        // There is either POLLIN or POLLOUT registered for a socket here.
        if (ufds[i].revents & POLLIN) {
          assert(callbacks_.count(static_cast<int>(ufds[i].fd)) > 0);
          callbacks_[static_cast<int>(ufds[i].fd)](operation::read);
        } else if (ufds[i].revents & POLLOUT) {
          assert(callbacks_.count(static_cast<int>(ufds[i].fd)) > 0);
          callbacks_[static_cast<int>(ufds[i].fd)](operation::write);
        }
      }
    }
  }
  // TODO: cleanup.
  return;
}

void driver::enable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      add_[sockfd](POLLIN);
      break;
    case operation::write:
      add_[sockfd](POLLOUT);
  }
}

void driver::disable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      del_[sockfd](POLLIN);
      break;
    case operation::write:
      del_[sockfd](POLLOUT);
  }
}

#else // defined(SPOKI_EPOLL_DECODER)

void driver::run() {
  aout(self_) << "running epoll\n";
  std::vector<epoll_event> pollset;
  pollset.resize(32);
  epollfd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epollfd_ < 0) {
    std::cerr << "epoll_create1: " << strerror(errno) << std::endl;
    exit(errno);
  }
  auto add = [this](auto sockfd, auto inital) {
    //std::cout << "registering event (" << inital << ") for sockfd: " << sockfd << std::endl;
    epoll_event ee;
    ee.events = inital;
    ee.data.fd = sockfd;
    masks_[sockfd] = inital;
    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, sockfd, &ee) < 0) {
      std::cerr << "epoll_ctl: " << strerror(errno) << std::endl;
      exit(errno);
    }
  };
  // Read event to notify queue changes, need to read these.
  add(notify_read_fd_, EPOLLIN);
  // Write from queue to decoder, not registered.
  add(decode_write_fd_, 0);
  // Read from decoder, read these.
  add(decode_read_fd_, EPOLLIN);
  // Read from scamper, read these.
  add(scamper_fd_, EPOLLIN | EPOLLOUT);
  // Let's start the loop.
  int cnt = -1;
  for (done_ = false; !done_;) {
    cnt = ::epoll_wait(epollfd_, pollset.data(),
                       static_cast<int>(pollset.size()), -1);
    if (cnt < 0) {
      switch (errno) {
        case EINTR: {
          // A signal was caught just try again.
          continue;
        }
        default: {
          perror("epoll_wait() failed");
          done_ = true;
        }
      }
    } else {
      //std::cout << "epoll returned " << cnt << " events\n";
      for (int i = 0; i < cnt; ++i) {
        //std::cout << "EPOLL EVENT\n";
        //std::cout << "socket: " << pollset[i].data.fd << std::endl;
        // There is either EPOLLIN or EPOLLOUT registered for a socket here.
        if (pollset[i].events & epoll_errors) {
          std::cerr << "epoll failed for socket: " << pollset[i].data.fd << ": "
                    << caf::io::network::last_socket_error_as_string()
                    << std::endl;
          disable(pollset[i].data.fd, operation::read);
          disable(pollset[i].data.fd, operation::write);
        } else if (pollset[i].events & EPOLLIN) {
          //std::cout << "it's a read event" << std::endl;
          assert(callbacks_.count(static_cast<int>(pollset[i].data.fd)) > 0);
          callbacks_[static_cast<int>(pollset[i].data.fd)](operation::read);
        } else if (pollset[i].events & EPOLLOUT) {
          //std::cout << "it's a write event" << std::endl;
          assert(callbacks_.count(static_cast<int>(pollset[i].data.fd)) > 0);
          callbacks_[static_cast<int>(pollset[i].data.fd)](operation::write);
        }
      }
    }
  }
  // TODO: cleanup.
  ::close(epollfd_);
  return;
}

void driver::mod(int sockfd, int operation, uint32_t events) {
  epoll_event ee;
  ee.events = events;
  ee.data.fd = sockfd;
  //std::cout << "epoll_ctl setting event (" << events << ") on " << sockfd << std::endl;
  if (epoll_ctl(epollfd_, operation, sockfd, &ee) < 0) {
    std::cerr << "epoll_ctl: " << strerror(errno) << std::endl;
    exit(errno);
  }
}

void driver::enable(int sockfd, operation op) {
  const auto old = masks_[sockfd];
  const auto change = (op == operation::read) ? EPOLLIN : EPOLLOUT;
  const auto mask = old | change;
  // const auto task = (old == 0) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
  const auto task = EPOLL_CTL_MOD;
  if (mask != old) {
    mod(sockfd, task, mask);
    masks_[sockfd] = mask;
  }
}

void driver::disable(int sockfd, operation op) {
  const auto old = masks_[sockfd];
  const auto change = (op == operation::read) ? EPOLLIN : EPOLLOUT;
  const auto mask = old & ~change;
  // const auto task = (old == 0) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
  const auto task = EPOLL_CTL_MOD;
  if (mask != old) {
    mod(sockfd, task, mask);
    masks_[sockfd] = mask;
  }
}

#endif // SPOKI_KQUEUE_DECODER

} // namespace spoki::scamper
