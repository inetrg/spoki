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

#include "spoki/scamper/broker.hpp"

#include <cassert>
#include <cstdlib>
#include <iomanip>
#include <limits>

// Need to convert some byte orders for warts.
#include <arpa/inet.h>

extern "C" {

// Because Scamper's include files are not self-contained this ordering is
// important so separate with newlines to prevent clang-format from re-arranging
// them.
#include <scamper_list.h>

#include <scamper_addr.h>
#include <scamper_file.h>
#include <scamper_writebuf.h>

#include <scamper_ping.h>

// Found in libscamperfile, but the header has a very generic name.
extern int uudecode_line(const char* in, size_t ilen, uint8_t* out,
                         size_t* olen);
extern int fcntl_set(const int fd, const int flags);

} // extern C

#include "spoki/collector.hpp"
#include "spoki/defaults.hpp"
#include "spoki/logger.hpp"
#include "spoki/probe/method.hpp"
#include "spoki/scamper/ec.hpp"
#include "spoki/scamper/reply.hpp"

using std::move;

namespace spoki::scamper {

namespace {

// The smallest payload send by scamper for regular commands.
constexpr auto max_msg_size = uint32_t{512};

// Attach to a running scamper daemon.
constexpr auto attach_cmd = "attach\n";
// Detach from a running scamper daemon.
constexpr auto done_cmd = "done\n";

// Indicates an error, followed by error message.
// constexpr auto attach_mode_err = "ERR";
// Indicates that scamper accepts the command, followed by id of the command.
// constexpr auto attach_mode_ok = "OK";
// Indicates that scamper has the capacity to accept more probing commands.
// constexpr auto attach_mode_more = "MORE";
// Indicates start of result, followed by a length which specifies the number
// of bytes in the data including newlines. Data is in binary warts format.
// constexpr auto attach_mode_data = "DATA ";

// Base for integer parsing.
constexpr auto base = 10;

// Try reconnecting after connection loss every ...
constexpr auto reconnect_timeout = std::chrono::seconds(15);

} // namespace

// -- SCAMPER INSTANCE ---------------------------------------------------------

instance::instance() : data_left(0), more(0) {
  // nop
}

// -- STATE INITIALIZATION -----------------------------------------------------

broker_state::broker_state(caf::io::broker* selfptr)
  : offset{0}, self{selfptr} {
  // nop
}

broker_state::~broker_state() {
  // nop
}

const char* broker_state::name = "scamper_broker";

// -- SCAMPER INTERACTION ------------------------------------------------------

void broker_state::send_requests() {
  if (handles.empty())
    return;
  for (size_t i = 0; i < handles.size(); ++i)
    send_requests(handles[i]);
  /*
  bool sent = false;
  // We want to avoid sending all requests to one instance if we only get
  // them one by one.
  do {
    sent = false;
    for (size_t i = offset; i < handles.size(); ++i)
      sent |= send_requests(handles[i]);
    for (size_t i = 0; i < offset; ++i)
      sent |= send_requests(handles[i]);
    offset = (offset + 1) % handles.size();
  } while (sent && !request_queue.empty());
  */
}

bool broker_state::send_requests(conn hdl) {
  auto& si = daemons[hdl];
  auto res = false;
  // while (si.more > 0 && !request_queue.empty()) {
  while (si.more > 0) {
    req.user_id = ++user_id_counter;
    daddr = (daddr + 1) & daddr_suffix_max;
    auto complete_daddr = (daddr << 8) | daddr_prefix;
    req.daddr = caf::ipv4_address::from_bits(complete_daddr);
    auto cmd = make_command(req);
    // aout(self) << "generated command: " << cmd;
    // aout(self) << "generated command size: " << cmd.size() << std::endl;
    // exit(1);
    self->write(hdl, cmd.size(), cmd.data());
    self->flush(hdl);
    --si.more;
    res = true;
    // request_queue.pop_front();
    ++stats_requested;
    ++stats_requested_per_broker[hdl];
  }
  return res;
}

void broker_state::attach(conn hdl) {
  CS_LOG_TRACE("");
  self->write(hdl, strlen(attach_cmd), attach_cmd);
  self->flush(hdl);
}

void broker_state::detach(conn hdl) {
  CS_LOG_TRACE("");
  self->write(hdl, strlen(done_cmd), done_cmd);
  self->flush(hdl);
}

bool broker_state::handle_data(conn hdl, const std::string_view& buf) {
  auto& si = daemons[hdl];
  assert(si.data_left > 0);
  std::vector<uint8_t> uu(64);
  auto len = uu.size();
  if (uudecode_line(buf.data(), buf.size(), uu.data(), &len) != 0) {
    std::cerr << "could not uudecode data: '" << buf << "'\n";
    return false;
  }
  if (len != 0) {
    uu.resize(len);
    si.dec->write(uu);
  }
  // The newline we cut off.
  si.data_left -= (buf.size() + 1);
  return true;
}

void broker_state::handle_reply(conn hdl, const std::string_view& buf) {
  auto& si = daemons[hdl];
  const char* head = buf.data();
  // auto line_len = buf.size();
  // Skip empty lines.
  if (head[0] == '\0') {
    return;
  }
  // Feedback letting us know that the command was accepted.
  // else if (line_len >= 2 && strncasecmp(head, attach_mode_ok, 2) == 0) {
  // else if (head[0] == 'O' && strncasecmp(head, attach_mode_ok, 2) == 0) {
  else if (head[0] == 'O') {
    return;
  }
  // If the scamper process is asking for more tasks, give it more.
  // else if (line_len == 4 && strncasecmp(head, attach_mode_more, line_len) ==
  // 0) { else if (head[0] == 'M' && strncasecmp(head, attach_mode_more,
  // line_len) == 0) {
  else if (head[0] == 'M') {
    si.more += 1;
    stats_more += 1;
    stats_more_per_broker[hdl] += 1;
    send_requests(hdl);
    return;
  }
  // New piece of data.
  // else if (line_len > 5 && strncasecmp(head, attach_mode_data, 5) == 0) {
  // else if (head[0] == 'D' && strncasecmp(head, attach_mode_data, 5) == 0) {
  else if (head[0] == 'D') {
    auto res = strtoul(head + 5, nullptr, base);
    if (res != 0)
      si.data_left = res;
    return;
  }
  // Feedback letting us know that the command was not accepted.
  // else if (line_len >= 3 && strncasecmp(head, attach_mode_err, 3) == 0) {
  // else if (head[0] == 'E' && strncasecmp(head, attach_mode_err, 3) == 0) {
  else if (head[0] == 'E') {
    aout(self) << std::string(buf.data(), buf.size()) << std::endl;
    return;
  } else {
    aout(self) << "received unknown command from scamper\n";
    return;
  }
}

// -- BEHAVIOR -----------------------------------------------------------------

caf::behavior broker(caf::io::stateful_broker<broker_state>* self,
                     const std::string& protocol_tag) {
  self->set_default_handler(caf::drop);
  auto dir = caf::get_or(self->system().config(), "collectors.out-dir", "");
  // TODO: Add config flag to control scamper responses logging.
  if (!dir.empty())
    self->state.reply_collector
      = self->spawn(spoki::collector, std::move(dir), "scamper-responses",
                    protocol_tag,
                    std::string{defaults::scamper_csv_header.begin(),
                                defaults::scamper_csv_header.end()},
                    static_cast<uint32_t>(self->id()));
  self->delayed_send(self, std::chrono::seconds(1), stats_atom_v);
  auto& s = self->state;
  s.stats_completed = 0;
  s.stats_more = 0;
  s.stats_new = 0;
  s.stats_requested = 0;
  s.stats_rst_completed = 0;
  s.stats_rst_new = 0;
  s.stats_rst_in_progress = 0;
  // These can only be initialized when we establish a connection.
  // std::unordered_map<conn, uint32_t> stats_more_per_broker;
  // std::unordered_map<conn, uint32_t> stats_requested_per_broker;
  // Build dummy packet for testing.
  s.user_id_counter = 0;
  s.req.probe_method = probe::method::tcp_synack;
  s.req.saddr = caf::ipv4_address::from_bits(0x01020308);
  s.req.sport = 1337;
  s.req.dport = 80;
  s.req.anum = 123881;
  s.req.num_probes = 1;
  return {
    // Add scamper daemon reachable via `host`:`port` for probing.
    [=](connect_atom, const std::string& host,
        uint16_t port) -> caf::result<std::string> {
      auto dec = async_decoder::make(self->system(), self);
      if (!dec) {
        CAF_LOG_ERROR("failed to initalize new warts decoder");
        return ec::failed_to_start_decoder;
      }
      auto econn = self->add_tcp_scribe(host, port);
      if (!econn) {
        CS_LOG_ERROR("could not connect to scamper on " << host << ":" << port);
        return ec::failed_to_connect;
      }
      aout(self) << "connected" << std::endl;
      auto conn = std::move(*econn);
      // Set a max message size, we basically want to get all we can.
      self->configure_read(conn,
                           caf::io::receive_policy::at_most(max_msg_size));
      self->state.daemons[conn] = instance();
      self->state.daemons[conn].dec = std::move(dec);
      // Attach to new scamper daemon.
      self->state.attach(conn);
      self->state.daemons[conn].host = host;
      self->state.daemons[conn].port = port;
      self->state.handles.push_back(conn);
      self->state.stats_more_per_broker[conn] = 0;
      self->state.stats_requested_per_broker[conn] = 0;
      // Scamper will send a more message to request work.
      return std::string{"success"};
    },
    [=](reconnect_atom, std::string& host, uint16_t port) {
      auto econn = self->add_tcp_scribe(host, port);
      if (!econn) {
        std::cerr << "failed to reconnect to scamper on '" << host << ":"
                  << port << "'" << std::endl;
        self->delayed_send(self, reconnect_timeout, reconnect_atom_v,
                           std::move(host), port);
        return;
      }
      auto dec = async_decoder::make(self->system(), self);
      if (!dec) {
        // CAF_LOG_ERROR("failed to initalize new warts decoder after
        // reconnect");
        std::cerr << "failed to initalize new warts decoder after reconnect"
                  << std::endl;
        self->close(*econn);
        self->delayed_send(self, reconnect_timeout, reconnect_atom_v,
                           std::move(host), port);
        return;
      }
      std::cerr << "reconnected to '" << host << ":" << port << "'"
                << std::endl;
      auto conn = std::move(*econn);
      // Set a max message size, we basically want to get all we can.
      self->configure_read(conn,
                           caf::io::receive_policy::at_most(max_msg_size));
      self->state.daemons[conn] = instance();
      self->state.daemons[conn].dec = std::move(dec);
      // Attach to new scamper daemon.
      self->state.attach(conn);
      self->state.daemons[conn].host = host;
      self->state.daemons[conn].port = port;
      self->state.handles.push_back(conn);
    },
    // The connection to scamper was lost.
    [=](const caf::io::connection_closed_msg& msg) {
      auto& s = self->state;
      auto& si = s.daemons[msg.handle];
      // CS_LOG_ERROR("lost connection to scamper at '" << si.host << ":"
      // << si.port<< "'");
      std::cerr << "lost connection to scamper at '" << si.host << ":"
                << si.port << "'" << std::endl;
      // Cleanup decoder.
      si.dec.reset();
      // Remove connection from active handles.
      auto& hdls = self->state.handles;
      auto hdl = std::find(hdls.begin(), hdls.end(), msg.handle);
      if (hdl != hdls.end())
        hdls.erase(hdl);
      // Trigger reconnect.
      self->send(self, reconnect_atom_v, si.host, si.port);
      // Remove state.
      self->state.daemons.erase(msg.handle);
    },
    // New data from scamper, might be a command or data to decode.
    [=](caf::io::new_data_msg& msg) {
      auto& s = self->state;
      auto& si = s.daemons[msg.handle];
      if (si.buf.empty()) {
        std::swap(msg.buf, si.buf);
      } else {
        aout(self) << "inserting bytes" << std::endl;
        si.buf.insert(si.buf.end(), msg.buf.begin(), msg.buf.end());
        // If we swap them at the end, this one should be empty.
        msg.buf.clear();
      }
      auto data_view = caf::make_span(reinterpret_cast<char*>(si.buf.data()),
                                      si.buf.size());
      auto from = data_view.begin();
      auto to = std::find(data_view.begin(), data_view.end(), '\n');
      while (to != data_view.end()) {
        auto len = std::distance(from, to);
        assert(static_cast<char>(*(from + len)) == '\n');
        assert(static_cast<char>(*(from + (len - 1))) != '\r');
        *to = '\0';
        std::string_view reply{from, static_cast<size_t>(len)};
        if (si.data_left > 0) {
          s.handle_data(msg.handle, reply);
        } else {
          s.handle_reply(msg.handle, reply);
        }
        from = to + 1;
        to = std::find(from, data_view.end(), '\n');
      }
      if (from == data_view.end()) {
        std::swap(msg.buf, si.buf);
      } else {
        aout(self) << "erasing bytes\n";
        auto len = std::distance(data_view.begin(), from);
        si.buf.erase(si.buf.begin(), si.buf.begin() + len);
      }
    },
    [=](probed_atom, reply& record) {
      auto& s = self->state;
      auto user_id = record.userid;
      auto unix_ts = record.start.sec;
      auto m = record.probe_method;
      self->send(s.reply_collector, std::move(record), unix_ts);
      if (s.in_progress.count(user_id) == 0) {
        aout(self) << "[scb] " << self->id() << " missing entry for " << user_id
                   << std::endl;
        return;
      }
      if (m == spoki::probe::method::tcp_rst) {
        s.stats_rst_completed += 1;
        s.stats_rst_in_progress -= 1;
      }
      // Forward to responsible shard.
      self->send(s.in_progress[user_id], probed_atom_v, user_id, m);
      // Remove and inc stats.
      s.in_progress.erase(user_id);
      s.stats_completed += 1;
    },
    // A new probe request for a single address.
    [=](request_atom, probe::request req) {
      auto& s = self->state;
      if (s.in_progress.count(req.user_id) > 0) {
        aout(self) << "probe to " << to_string(req.daddr) << " with tag "
                   << req.user_id << " already in progress (wrap around?)"
                   << std::endl;
        self->send(caf::actor_cast<caf::actor>(self->current_sender()),
                   probed_atom_v, req.user_id, req.probe_method);
        return;
      }
      if (req.probe_method == spoki::probe::method::tcp_rst) {
        s.stats_rst_new += 1;
        s.stats_rst_in_progress += 1;
      }
      // s.in_progress[req.user_id]
      //= caf::actor_cast<caf::actor>(self->current_sender());
      s.request_queue.push_back(std::move(req));
      s.stats_new += 1;
      s.send_requests();
    },
    [=](stats_atom) {
      auto& s = self->state;
      self->delayed_send(self, std::chrono::seconds(1), stats_atom_v);
      std::stringstream ss;
      ss << " n: " << s.stats_new << " m: " << s.stats_more
         << " r: " << s.stats_requested << " q: " << s.request_queue.size();
      ss << " (mpb:";
      for (auto& elem : s.stats_more_per_broker)
        ss << " [" << to_string(elem.first) << ": " << elem.second << "]";
      ss << ")";
      ss << " (rpb:";
      for (auto& elem : s.stats_requested_per_broker)
        ss << "[" << to_string(elem.first) << ": " << elem.second << "]";
      ss << ")";
      ss << std::endl;
      aout(self) << ss.str();
      s.stats_completed = 0;
      s.stats_more = 0;
      s.stats_new = 0;
      s.stats_requested = 0;
      s.stats_rst_completed = 0;
      s.stats_rst_new = 0;
      for (auto& elem : s.stats_more_per_broker)
        elem.second = 0;
      for (auto& elem : s.stats_requested_per_broker)
        elem.second = 0;
    },
    // The actor is done. Detach from scamper and terminate.
    [=](done_atom) {
      for (auto& h : self->state.handles)
        self->close(h);
      self->quit();
    },
  };
}

} // namespace spoki::scamper
