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

#include "spoki/probe/udp_prober.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <iomanip>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <caf/io/network/default_multiplexer.hpp>
#include <caf/io/network/interfaces.hpp>
#include <caf/io/network/ip_endpoint.hpp>
#include <caf/io/network/native_socket.hpp>
#include <caf/ipv4_address.hpp>

#ifdef SPOKI_KQUEUE_DECODER
#  include <sys/event.h>
#else // SPOKI_POLL_DECODER
#  include <poll.h>

#endif
#include "spoki/atoms.hpp"
#include "spoki/logger.hpp"
#include "spoki/task.hpp"

#include "spoki/scamper/ping.hpp"

namespace spoki::probe {

namespace {

void down_handler(caf::scheduled_actor* ptr, caf::down_msg& msg) {
  if (msg.reason != caf::exit_reason::user_shutdown)
    aout(ptr) << "shard died unexpectedly (" << to_string(msg.source)
              << "): " << to_string(msg.reason) << std::endl;
}

void poke(int sockfd) {
  uint8_t tmp = 1;
  ::send(sockfd, &tmp, sizeof(tmp), 0);
}

struct pseudo_ip_hdr {
  uint8_t ihl : 4, ver : 4;
  uint8_t ecn : 2, tos : 6;
  uint16_t len;
  uint16_t idn;
  uint16_t off; // off: 13, flg: 3; this doesn't work as I want it to
  uint8_t ttl;
  uint8_t pro;
  uint16_t chk;
  uint32_t src;
  uint32_t dst;
};

struct pseudo_udp_hdr {
  uint16_t src;
  uint16_t dst;
  uint16_t len;
  uint16_t chk;
};

uint16_t checksum(const pseudo_ip_hdr* hdr) {
  const uint16_t* begin = reinterpret_cast<const uint16_t*>(hdr);
  const uint16_t* end = begin + sizeof(pseudo_ip_hdr) / 2;
  uint32_t checksum = 0;
  uint32_t first_half = 0;
  uint32_t second_half = 0;

  for (; begin != end; ++begin)
    checksum += *begin;

  first_half = static_cast<uint16_t>(checksum >> 16);
  while (first_half != 0) {
    second_half = static_cast<uint16_t>((checksum << 16) >> 16);
    checksum = first_half + second_half;
    first_half = static_cast<uint16_t>(checksum >> 16);
  }

  return static_cast<uint16_t>(~checksum);
}

} // namespace

udp_request::udp_request(caf::ipv4_address saddr, caf::ipv4_address daddr,
                         uint16_t sport, uint16_t dport, std::vector<char> pl)
  : saddr{saddr},
    daddr{daddr},
    sport{sport},
    dport{dport},
    payload(std::move(pl)) {
  // nop
}

udp_prober::udp_prober(int sockfd, payload_map payloads)
  : reflect_{false},
    default_payload_{'\x0A'},
    // default_payload_{'\x0D', '\x0A', '\x0D', '\x0A'},
    // default_payload_{'h', 'a', 'l', 'l', 'o', '\x0A'},
    payloads_{std::move(payloads)},
    done_{false},
    writing_{false},
    probe_out_fd_{sockfd} {
  // nop
}

udp_prober::~udp_prober() {
  // Close the open sockets.
  close(notify_in_fd_);
  close(notify_out_fd_);
  close(probe_out_fd_);
}

udp_prober::udp_prober_ptr udp_prober::make(bool service_specific,
                                            bool reflect) {
  using namespace caf::io;
  auto fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (fd == -1) {
    std::cerr << "ERR: Failed to create raw socket: "
              << network::last_socket_error_as_string() << std::endl;
    return nullptr;
  }
  // I think this is already enabled on IPPROTO_RAW.
  int on = 1;
  if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL,
                 reinterpret_cast<network::setsockopt_ptr>(&on),
                 static_cast<network::socket_size_type>(sizeof(on)))) {
    std::cerr << "ERR: Failed to set IP_HDRINCL: " << strerror(errno)
              << std::endl;
    close(fd);
    exit(0);
  }
  /*
  int on = 1;
  auto ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                        reinterpret_cast<network::setsockopt_ptr>(&on),
                        static_cast<network::socket_size_type>(sizeof(on)));
  if (ret != 0) {
    std::cerr << "ERR: Failed to set reuse address option." << std::endl;
    close(sockfd);
    return nullptr;
  }
  */
  // Create socket pair for notifcation of the mpx loop.
  std::array<int, 2> notify_pair;
  // Create socket pair for notification.
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, notify_pair.data()) != 0) {
    std::cerr << "ERR: Failed to create pair for notification." << std::endl;
    close(fd);
    return nullptr;
  }
  // Set nonblocking.
  if (!network::nonblocking(fd, true)
      || !network::nonblocking(notify_pair[0], true)
      || !network::nonblocking(notify_pair[1], true)) {
    std::cerr << "ERR: Failed to set sockets to non-blocking." << std::endl;
    close(fd);
    close(notify_pair[0]);
    close(notify_pair[1]);
    return nullptr;
  }
  // Generate payload map.
  payload_map payloads;
  if (service_specific)
    payloads = get_payloads();
  // Nice, let's get this started.
  auto ptr = caf::make_counted<udp_prober>(fd, std::move(payloads));
  ptr->notify_in_fd_ = notify_pair[0];
  ptr->notify_out_fd_ = notify_pair[1];
  ptr->reflect_ = reflect;
  ptr->start();
  return ptr;
}

void udp_prober::start() {
  // Set callbacks for socket event.
  callbacks_.clear();
  callbacks_[notify_in_fd_] = [&]() { handle_notify_read(); };
  callbacks_[probe_out_fd_] = [&]() { handle_probe_write(); };
  mpx_loop_ = std::thread(&udp_prober::run, this);
}

void udp_prober::add_request(caf::ipv4_address saddr, caf::ipv4_address daddr,
                             uint16_t sport, uint16_t dport,
                             std::vector<char>& pl) {
  // This might be called from another thread.
  std::lock_guard<std::mutex> guard(mtx_);
  requests_.emplace_back(saddr, daddr, sport, dport, std::move(pl));
  if (!writing_)
    poke(notify_out_fd_);
}

void udp_prober::shutdown() {
  // Not sure this will work ...
  stop();
}

void udp_prober::stop() {
  std::cout << "stop called" << std::endl;
  done_ = true;
  if (is_valid(notify_out_fd_)) {
    poke(notify_out_fd_);
    // TODO: This might be a bad idea?
    if (mpx_loop_.joinable())
      mpx_loop_.join();
  }
  mpx_loop_ = std::thread();
}

void udp_prober::handle_notify_read() {
  std::array<uint8_t, 16> tmp;
  auto res = ::recv(notify_in_fd_, tmp.data(), 16, 0);
  if (res < 0)
    std::cerr << "notify error: " << strerror(errno) << std::endl;
  // TODO: Check queue size?
  enable(probe_out_fd_, operation::write);
  writing_ = true;
}

void udp_prober::handle_probe_write() {
  using namespace caf::io::network;
  std::lock_guard<std::mutex> guard(mtx_);
  if (!requests_.empty()) {
    auto& next = requests_.front();
    // TODO: there is probably a better way to do this.
    //  The bits in the ipv4_address might already be in network byte order.
    auto ip_dst = to_string(next.daddr);
    auto ip_src = to_string(next.saddr);
    auto len = sizeof(pseudo_ip_hdr) + sizeof(pseudo_udp_hdr);
    auto& payload = default_payload_;
    if (payloads_.count(next.dport) > 0)
      payload = payloads_[next.dport];
    else if (reflect_)
      payload = next.payload;
    len += payload.size();
    send_buffer_.resize(len);
    memset(send_buffer_.data(), 0, send_buffer_.size());

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(next.dport);
    sin.sin_addr.s_addr = inet_addr(ip_dst.c_str());

    // Write IP header.
    auto* ip = reinterpret_cast<pseudo_ip_hdr*>(send_buffer_.data());
    ip->ver = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->ecn = 0;
    ip->len = htons(len);
    ip->idn = htons(1337);
    ip->off = htons(0x4000); // Don't fragment.
    ip->ttl = 64;
    ip->pro = IPPROTO_UDP;
    ip->src = inet_addr(ip_src.c_str());
    ip->dst = sin.sin_addr.s_addr;
    // Checksum at the end.
    ip->chk = htons(checksum(ip));

    // Write UDP header.
    auto udp_len = static_cast<uint16_t>(sizeof(pseudo_udp_hdr)
                                         + payload.size());
    auto* udp = reinterpret_cast<pseudo_udp_hdr*>(send_buffer_.data()
                                                  + sizeof(pseudo_ip_hdr));
    udp->src = htons(next.sport);
    udp->dst = htons(next.dport);
    udp->len = htons(udp_len);
    udp->chk = 0;

    // Attach payload.
    std::copy(payload.begin(), payload.end(),
              send_buffer_.data() + sizeof(pseudo_ip_hdr)
                + sizeof(pseudo_udp_hdr));

    // And send it!
    auto sres = ::sendto(probe_out_fd_,
                         reinterpret_cast<socket_send_ptr>(send_buffer_.data()),
                         send_buffer_.size(), 0,
                         reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    if (is_error(sres, true))
      std::cerr << "Failed to send to '" << ip_dst << ":" << next.sport
                << "': " << sres << std::endl;

    requests_.pop_front();
  }
  // Remove us from write events if there is nothing to process.
  if (requests_.empty()) {
    disable(probe_out_fd_, operation::write);
    writing_ = false;
  }
}

void udp_prober::handle_probe_read() {
  // nop, shouldn't happen.
}

bool udp_prober::is_valid(int sockfd) {
  return fcntl(sockfd, F_GETFL) != -1 || errno != EBADF;
}

#ifdef SPOKI_KQUEUE_DECODER

void udp_prober::run() {
  // Multiplexer ref.
  caf::intrusive_ptr_add_ref(this);
  // Create kqueue for multiplexing.
  if ((kq_ = kqueue()) == -1) {
    std::cerr << "failed to create kqueue\n";
    return;
  }
  // Setup our initial socket events;
  std::array<struct kevent, 2> initial_events;
  EV_SET(&initial_events[0], static_cast<uint64_t>(notify_in_fd_), EVFILT_READ,
         EV_ADD | EV_ENABLE, 0, 0, nullptr);
  // We only want to enable this once we have data to write.
  EV_SET(&initial_events[1], static_cast<uint64_t>(probe_out_fd_), EVFILT_WRITE,
         EV_ADD | EV_DISABLE, 0, 0, nullptr);
  if (kevent(kq_, initial_events.data(), 3, nullptr, 0, nullptr) == -1) {
    std::cerr << "failed to setup initial kqueue events\n";
    return;
  }
  // For our kqueue operation;
  std::array<struct kevent, 32> events;
  for (; !done_;) {
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
        std::cerr << "got error event with "
                     << events[i].data << ": "
                     << strerror(static_cast<int>(events[i].data)) << std::endl;
        done_ = true;
      } else if (callbacks_.count(static_cast<int>(events[i].ident)) > 0) {
        callbacks_[static_cast<int>(events[i].ident)]();
      } else {
        std::cerr << "kqueue returned unexpected event\n";
      }
    }
  }
  // Remove all the events.
  EV_SET(&initial_events[0], static_cast<uint64_t>(notify_in_fd_), EVFILT_READ,
         EV_DELETE, 0, 0, nullptr);
  EV_SET(&initial_events[1], static_cast<uint64_t>(probe_out_fd_), EVFILT_WRITE,
         EV_DELETE, 0, 0, nullptr);
  if (kevent(kq_, initial_events.data(), 3, nullptr, 0, nullptr) == -1) {
    std::cerr << "failed to setup initial kqueue events\n";
    return;
  }
  if (kq_)
    close(kq_);
  // Multiplexer ref.
  caf::intrusive_ptr_release(this);
}

void udp_prober::enable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      mod(static_cast<uint64_t>(sockfd), EVFILT_READ, EV_ENABLE);
      break;
    case operation::write:
      mod(static_cast<uint64_t>(sockfd), EVFILT_WRITE, EV_ENABLE);
  }
}

void udp_prober::disable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      mod(static_cast<uint64_t>(sockfd), EVFILT_READ, EV_DISABLE);
      break;
    case operation::write:
      mod(static_cast<uint64_t>(sockfd), EVFILT_WRITE, EV_DISABLE);
  }
}

void udp_prober::mod(uint64_t sockfd, int16_t filter, uint16_t operation) {
  struct kevent new_event;
  EV_SET(&new_event, sockfd, filter, operation, 0, 0, nullptr);
  if (kevent(kq_, &new_event, 1, nullptr, 0, nullptr) == -1) {
    std::cerr << "failed to add event to kqueue\n";
    done_ = true;
  }
}

#else // SPOKI_POLL_DECODER

void udp_prober::run() {
  // Multiplexer ref.
  caf::intrusive_ptr_add_ref(this);
  std::array<pollfd, 2> ufds;
  // read event to notify queue changes
  ufds[0].fd = notify_in_fd_;
  ufds[0].events = POLLIN;
  mods_[notify_in_fd_] = [&](short val) { ufds[0].events = val; };
  // write to socket from queue
  ufds[1].fd = probe_out_fd_;
  ufds[1].events = 0;
  mods_[probe_out_fd_] = [&](short val) { ufds[1].events = val; };
  int rv = -1;
  const auto cnt = ufds.size();
  const auto mask = POLLIN | POLLOUT;
  for (done_ = false; !done_;) {
    rv = ::poll(ufds.data(), ufds.size(), -1);
    if (rv == -1) {
      std::cerr << "poll returned " << rv << std::endl;
      done_ = true;
      continue;
    }
    for (size_t i = 0; i < cnt; ++i) {
      // There is either POLLIN or POLLOUT registered for a socket here.
      if (ufds[i].revents & mask) {
        if (callbacks_.count(static_cast<int>(ufds[i].fd)) > 0) {
          callbacks_[static_cast<int>(ufds[i].fd)]();
        } else {
          //std::cerr << "no callback found" << std::endl;
          std::cerr << "poll returned unexpected event\n";
        }
      }
    }
  }
  // Multiplexer ref.
  caf::intrusive_ptr_release(this);
}

void udp_prober::enable(int sockfd, operation op) {
  switch (op) {
    case operation::read:
      mods_[sockfd](POLLIN);
      break;
    case operation::write:
      mods_[sockfd](POLLOUT);
  }
}

void udp_prober::disable(int sockfd, operation) {
  mods_[sockfd](0);
}

#endif // SPOKI_POLL_DECODER

const char* upi_state::name = "udp_prober";

caf::behavior prober(caf::stateful_actor<upi_state>* self,
                     udp_prober::udp_prober_ptr backend) {
  self->state.backend = backend;
  self->set_down_handler(down_handler);
  self->set_default_handler(caf::print_and_drop);
  return {
    [=](spoki::request_atom, packet& pkt) {
      if (pkt.carries_udp()) {
        auto& proto = pkt.protocol<net::udp>();
        self->state.backend->add_request(pkt.daddr, pkt.saddr, proto.dport,
                                         proto.sport, proto.payload);
      } else {
        std::cerr << "Not UDP, dropping " << to_string(pkt) << std::endl;
      }
    },
    [=](spoki::request_atom, caf::ipv4_address& saddr, caf::ipv4_address& daddr,
        uint16_t sport, uint16_t dport, std::vector<char> pl) {
      aout(self) << "New target: " << to_string(daddr) << ":" << dport
                 << std::endl;
      self->state.backend->add_request(saddr, daddr, sport, dport, pl);
    },
    [=](spoki::done_atom) {
      aout(self) << "Prober got done message" << std::endl;
      self->state.backend->shutdown();
      self->quit();
    },
  };
}

} // namespace spoki::probe
