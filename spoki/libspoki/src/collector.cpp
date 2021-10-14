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

#include "spoki/collector.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <caf/ipv4_address.hpp>

#include <nlohmann/json.hpp>

#include "spoki/analysis/classification.hpp"
#include "spoki/atoms.hpp"
#include "spoki/packet.hpp"
#include "spoki/probe/payloads.hpp"
#include "spoki/probe/request.hpp"
#include "spoki/scamper/reply.hpp"
#include "spoki/time.hpp"

using json = nlohmann::json;

namespace {

bool is_directory(const std::string& dir) {
  if (dir.empty()) {
    return false;
  }
  if (::access(dir.c_str(), 0) != 0) {
    return false;
  }
  struct stat status;
  stat(dir.c_str(), &status);
  if (status.st_mode & S_IFDIR) {
    return true;
  }
  return false;
}

bool exists(const std::string& filename) {
  if (filename.empty()) {
    return false;
  }
  std::ifstream fin(filename.c_str());
  return fin.good();
}

constexpr std::time_t secs_per_hour = 3600;

constexpr char delimiter = '|';
constexpr std::string_view icmp_field = "icmp";
constexpr std::string_view tcp_field = "tcp";
constexpr std::string_view udp_field = "udp";

// -- line appending -----------------------------------------------------------

void append_field_del(std::vector<char>& buf, const std::string str) {
  buf.insert(buf.end(), str.begin(), str.end());
  buf.emplace_back(delimiter);
}

void append_field(std::vector<char>& buf, const std::string str) {
  buf.insert(buf.end(), str.begin(), str.end());
}

std::string
to_string(const std::unordered_map<spoki::net::tcp_opt,
                                   caf::optional<uint32_t>>& options) {
  std::string str = "";
  for (auto& [key, val] : options) {
    if (!str.empty()) {
      str += ":";
    }
    str += spoki::net::option_name(key);
  }
  return str;
}

/// Get the protocol enum value.
struct vis_protocol_string {
  // proto|sport|dport|anum|snum|options|payload|syn|ack|rst|fin|window size
  void operator()(const spoki::net::icmp& pkt) const {
    buf.insert(buf.end(), icmp_field.begin(), icmp_field.end());
    buf.emplace_back(delimiter);
    // skip sport
    buf.emplace_back(delimiter);
    // skip dport
    buf.emplace_back(delimiter);
    // skip anum
    buf.emplace_back(delimiter);
    // skip snum
    buf.emplace_back(delimiter);
    append_field(buf, to_string(pkt.type)); // options
    buf.emplace_back(delimiter);
    // skip payload
    buf.emplace_back(delimiter);
    // skip syn
    buf.emplace_back(delimiter);
    // skip ack
    buf.emplace_back(delimiter);
    // skip rst
    buf.emplace_back(delimiter);
    // skip fin
    buf.emplace_back(delimiter);
    // skip window size
  }

  void operator()(const spoki::net::tcp& pkt) const {
    buf.insert(buf.end(), tcp_field.begin(), tcp_field.end());
    buf.emplace_back(delimiter);
    append_field_del(buf, std::to_string(pkt.sport));                // sport
    append_field_del(buf, std::to_string(pkt.dport));                // dport
    append_field_del(buf, std::to_string(pkt.anum));                 // anum
    append_field_del(buf, std::to_string(pkt.snum));                 // snum
    append_field_del(buf, to_string(pkt.options));                   // options
    append_field_del(buf, spoki::probe::to_hex_string(pkt.payload)); // payload
    append_field_del(buf, std::to_string(pkt.syn));                  // syn
    append_field_del(buf, std::to_string(pkt.ack));                  // ack
    append_field_del(buf, std::to_string(pkt.rst));                  // rst
    append_field_del(buf, std::to_string(pkt.fin));                  // fin
    append_field(buf, std::to_string(pkt.window_size)); // window size
  }

  void operator()(const spoki::net::udp& pkt) const {
    buf.insert(buf.end(), udp_field.begin(), udp_field.end());
    buf.emplace_back(delimiter);
    append_field(buf, std::to_string(pkt.sport)); // sport
    buf.emplace_back(delimiter);
    append_field(buf, std::to_string(pkt.dport)); // dport
    // skip anum
    buf.emplace_back(delimiter);
    // skip snum
    buf.emplace_back(delimiter);
    // skip options
    buf.emplace_back(delimiter);
    append_field(buf, spoki::probe::to_hex_string(pkt.payload)); // payload
    buf.emplace_back(delimiter);
    // skip syn
    buf.emplace_back(delimiter);
    // skip ack
    buf.emplace_back(delimiter);
    // skip rst
    buf.emplace_back(delimiter);
    // skip fin
    buf.emplace_back(delimiter);
    // skip window size
  }

  std::vector<char>& buf;
};

inline std::time_t align_to_hour(std::time_t ts) {
  return (ts - (ts % secs_per_hour));
}

} // namespace

namespace spoki {

const char* collector_state::name = "collector";

std::string collector_state::generate_file_name(std::time_t ts) {
  auto aligned_ts = align_to_hour(ts);
  std::array<char, 80> pretty_str;
  auto* timeinfo = localtime(&aligned_ts);
  strftime(pretty_str.data(), pretty_str.size(), "%F.%T", timeinfo);
  std::string filename = dir;
  filename += pretty_str.data();
  filename += ".";
  filename += datasource_tag;
  filename += ".spoki.";
  filename += protocol_tag;
  filename += ".";
  filename += component_tag;
  filename += ".";
  filename += std::to_string(align_to_hour(ts));
  // filename += ".";
  // filename += std::to_string(id);
  filename += ".csv";
  return filename;
}

void collector_state::open_log_file(out_file& of, time_t unix_ts) {
  auto current_hour_ts = align_to_hour(unix_ts);
  // Open file for this hour. Let's just assume it's the current hour.
  auto fn = generate_file_name(unix_ts);
  // std::cout << "opening logfile: " << fn << std::endl;
  auto file_already_exists = exists(fn);
  of.out.open(fn, std::fstream::out | std::fstream::app);
  of.start = current_hour_ts;
  of.end = current_hour_ts + secs_per_hour;
  of.filename = std::move(fn);
  if (!file_already_exists) {
    of.out.write(header.data(), header.size());
  }
}

// Implemented below.
caf::behavior collector_idle(caf::stateful_actor<collector_state>* self);
caf::behavior running_one_log(caf::stateful_actor<collector_state>*);
caf::behavior running_two_logs(caf::stateful_actor<collector_state>*);

// -- start & configuration ---------------------------------------------------

caf::behavior collector(caf::stateful_actor<collector_state>* self,
                        std::string dir, std::string component,
                        std::string protocol, std::string header, uint32_t id) {
  if (!is_directory(dir)) {
    CAF_LOG_ERROR("cannot write to directory '" << dir << "'");
    return {};
  }
  self->state.dir = std::move(dir);
  if (self->state.dir.back() != '/')
    self->state.dir += '/';
  auto uri = caf::get_or(self->config(), "uri", "unset");
  auto datasource = caf::get_or(self->config(), "collectors.datasource-tag",
                                "untagged");
  self->state.datasource_tag = std::move(datasource);
  self->state.protocol_tag = std::move(protocol);
  self->state.component_tag = std::move(component);
  self->state.interval = std::chrono::minutes(60);
  self->state.header = header;
  self->state.id = id;
  return collector_idle(self);
}

// -- CSV log lines ------------------------------------------------------------

void append_log_entry(std::vector<char>& buf, const scamper::reply& repl) {
  // start sec|start usec|method|userid|num probes|saddr|daddr|sport|dport
  append_field_del(buf, std::to_string(repl.start.sec));
  append_field_del(buf, std::to_string(repl.start.usec));
  append_field_del(buf, probe::probe_name(repl.probe_method));
  append_field_del(buf, std::to_string(repl.userid));
  append_field_del(buf, std::to_string(repl.ping_sent));
  append_field_del(buf, repl.src);
  append_field_del(buf, repl.dst);
  append_field_del(buf, std::to_string(repl.sport));
  append_field(buf, std::to_string(repl.dport));
}

void append_log_entry(std::vector<char>& buf, const packet& pkt) {
  // ts|saddr|daddr|ipid|ttl|proto|sport|dport|anum|snum|options|payload|syn|
  // ack|rst|fin|window size|probed|method|userid|probe anum|probe snum|
  // num probes
  append_field_del(buf, std::to_string(to_count(pkt.observed))); // ts
  append_field_del(buf, to_string(pkt.saddr));
  append_field_del(buf, to_string(pkt.daddr));     // daddr
  append_field_del(buf, std::to_string(pkt.ipid)); // ipid
  append_field_del(buf, std::to_string(pkt.ttl));  // ttl
  caf::visit(vis_protocol_string{buf}, pkt.proto); // proto -> window size
  buf.emplace_back(delimiter);
  append_field_del(buf, "false"); // probed
  // skip method
  buf.emplace_back(delimiter);
  // skip userid
  buf.emplace_back(delimiter);
  // skip probe anum
  buf.emplace_back(delimiter);
  // skip probe snum
  buf.emplace_back(delimiter);
  // skip num probes
}

void append_log_entry(std::vector<char>& buf, const packet& pkt,
                      const probe::request& req) {
  // ts|saddr|daddr|ipid|ttl|proto|sport|dport|anum|snum|options|payload|syn|
  // ack|rst|fin|window size|probed|method|userid|probe anum|probe snum|
  // num probes
  append_field_del(buf, std::to_string(to_count(pkt.observed))); // ts
  append_field_del(buf, to_string(pkt.saddr));                   // saddr
  append_field_del(buf, to_string(pkt.daddr));                   // daddr
  append_field_del(buf, std::to_string(pkt.ipid));               // ipid
  append_field_del(buf, std::to_string(pkt.ttl));                // ttl
  caf::visit(vis_protocol_string{buf}, pkt.proto); // proto -> window size
  buf.emplace_back(delimiter);
  append_field_del(buf, "true");                              // probed
  append_field_del(buf, probe::probe_name(req.probe_method)); // method
  append_field_del(buf, std::to_string(req.user_id));         // userid
  append_field_del(buf, std::to_string(req.anum));            // probe anum
  append_field_del(buf, std::to_string(req.snum));            // probe snum
  append_field(buf, std::to_string(req.num_probes));          // num probes
}

// -- writing -----------------------------------------------------------------

void collector_state::write_idle(std::string line, std::time_t unix_ts) {
  auto current_hour_ts = align_to_hour(unix_ts);
  // Whatever we get now is our current hour.
  open_log_file(current_hour, unix_ts);
  // Don't forget to log the event!
  current_hour.out << line << std::endl;
  // Calculate data for last hour.
  last_hour.start = current_hour_ts - secs_per_hour;
  last_hour.end = current_hour_ts;
  self->become(running_one_log(self));
}

void collector_state::write_one_log(std::string line, std::time_t unix_ts) {
  if (unix_ts >= current_hour.start && unix_ts < current_hour.end) {
    current_hour.out << line << std::endl;
  } else if (unix_ts >= current_hour.end) {
    // Timestamp is in the future.
    if (unix_ts < current_hour.end + secs_per_hour) {
      // And within the next hour: Rotate logs, open next hour file, switch to
      // next behavior.
      last_hour.start = current_hour.start;
      last_hour.end = current_hour.end;
      last_hour.out = std::move(current_hour.out);
      last_hour.filename = std::move(current_hour.filename);
      // Open a new log for the current hour.
      open_log_file(current_hour, unix_ts);
      // Don't forget to log our event!
      current_hour.out << line << std::endl;
      self->become(running_two_logs(self));
    } else {
      // Some more time in the future --> open that hour as the only log.
      current_hour.out.close();
      // Reopen for new "current hour".
      open_log_file(current_hour, unix_ts);
      current_hour.out << line << std::endl;
      // Calculate data for last and next hour.
      auto current_hour_ts = align_to_hour(unix_ts);
      last_hour.start = current_hour_ts - secs_per_hour;
      last_hour.end = current_hour_ts;
    }
  } else if (unix_ts >= last_hour.start && unix_ts < last_hour.end) {
    // Open file for last hour, next behavior.
    auto fn = generate_file_name(unix_ts);
    last_hour.out.open(fn, std::fstream::out | std::fstream::app);
    last_hour.filename = std::move(fn);
    last_hour.out << line << std::endl;
    self->become(running_two_logs(self));
  } else {
    // MUST be older than last hour.
    aout(self) << "ERR unexpected timestamp in write one log: " << unix_ts
               << " doesn't fit into [" << last_hour.start << ", "
               << last_hour.end << "), [" << current_hour.start << ", "
               << current_hour.end << ")" << std::endl;
  }
}

void collector_state::write_two_logs(std::string line, std::time_t unix_ts) {
  if (unix_ts >= current_hour.start && unix_ts < current_hour.end) {
    current_hour.out << line << std::endl;
  } else if (unix_ts >= last_hour.start && unix_ts < last_hour.end) {
    last_hour.out << line << std::endl;
  } else if (unix_ts >= current_hour.end) {
    // Timestamp is in the future.
    if (unix_ts < current_hour.end + secs_per_hour) {
      // And within the next hour: Rotate logs & open new file for current hour.
      last_hour.out.close();
      // Move current hour over.
      last_hour.start = current_hour.start;
      last_hour.end = current_hour.end;
      last_hour.out = std::move(current_hour.out);
      last_hour.filename = std::move(current_hour.filename);
      // Open a new log for the current hour.
      open_log_file(current_hour, unix_ts);
      // Don't forget to log our event!
      current_hour.out << line << std::endl;
    } else {
      // And more than an hour in the future. Close our logs, open a log for the
      // current hour, and return to the one-logfile state.
      last_hour.out.close();
      current_hour.out.close();
      open_log_file(current_hour, unix_ts);
      // Don't forget to log the event!
      current_hour.out << line << std::endl;
      // Calculate data for last hour.
      auto current_hour_ts = align_to_hour(unix_ts);
      last_hour.start = current_hour_ts - secs_per_hour;
      last_hour.end = current_hour_ts;
      self->become(running_one_log(self));
    }
  } else {
    // MUST be older than last hour.
    aout(self) << "ERR unexpected timestamp in write two logs: " << unix_ts
               << " doesn't fit into [" << last_hour.start << ", "
               << last_hour.end << "), [" << current_hour.start << ", "
               << current_hour.end << ")" << std::endl;
  }
}

// -- buffered writing --------------------------------------------------------

void collector_state::buffered_write_idle(const std::vector<char>& buf,
                                          std::time_t unix_ts) {
  auto current_hour_ts = align_to_hour(unix_ts);
  // Whatever we get now is our current hour.
  open_log_file(current_hour, unix_ts);
  // Don't forget to log the event!
  current_hour.out.write(buf.data(), buf.size());
  // Calculate data for last hour.
  last_hour.start = current_hour_ts - secs_per_hour;
  last_hour.end = current_hour_ts;
  self->become(running_one_log(self));
}

void collector_state::buffered_write_one_log(const std::vector<char>& buf,
                                             std::time_t unix_ts) {
  if (unix_ts >= current_hour.start && unix_ts < current_hour.end) {
    current_hour.out.write(buf.data(), buf.size());
  } else if (unix_ts >= current_hour.end) {
    // Timestamp is in the future.
    if (unix_ts < current_hour.end + secs_per_hour) {
      // And within the next hour: Rotate logs, open next hour file, switch to
      // next behavior.
      last_hour.start = current_hour.start;
      last_hour.end = current_hour.end;
      last_hour.out = std::move(current_hour.out);
      last_hour.filename = std::move(current_hour.filename);
      // Open a new log for the current hour.
      open_log_file(current_hour, unix_ts);
      // Don't forget to log our event!
      current_hour.out.write(buf.data(), buf.size());
      self->become(running_two_logs(self));
    } else {
      // Some more time in the future --> open that hour as the only log.
      current_hour.out.close();
      // Reopen for new "current hour".
      open_log_file(current_hour, unix_ts);
      current_hour.out.write(buf.data(), buf.size());
      // Calculate data for last and next hour.
      auto current_hour_ts = align_to_hour(unix_ts);
      last_hour.start = current_hour_ts - secs_per_hour;
      last_hour.end = current_hour_ts;
    }
  } else if (unix_ts >= last_hour.start && unix_ts < last_hour.end) {
    // Open file for last hour, next behavior.
    auto fn = generate_file_name(unix_ts);
    open_log_file(last_hour, unix_ts);
    // last_hour.out.open(fn, std::fstream::out | std::fstream::app);
    last_hour.filename = std::move(fn);
    last_hour.out.write(buf.data(), buf.size());
    self->become(running_two_logs(self));
  } else {
    // MUST be older than last hour.
    aout(self) << "ERR unexpected timestamp in write one log: " << unix_ts
               << " doesn't fit into [" << last_hour.start << ", "
               << last_hour.end << "), [" << current_hour.start << ", "
               << current_hour.end << ")" << std::endl;
  }
}

void collector_state::buffered_write_two_logs(const std::vector<char>& buf,
                                              std::time_t unix_ts) {
  if (unix_ts >= current_hour.start && unix_ts < current_hour.end) {
    current_hour.out.write(buf.data(), buf.size());
  } else if (unix_ts >= last_hour.start && unix_ts < last_hour.end) {
    last_hour.out.write(buf.data(), buf.size());
  } else if (unix_ts >= current_hour.end) {
    // Timestamp is in the future.
    if (unix_ts < current_hour.end + secs_per_hour) {
      // And within the next hour: Rotate logs & open new file for current hour.
      last_hour.out.close();
      // Move current hour over.
      last_hour.start = current_hour.start;
      last_hour.end = current_hour.end;
      last_hour.out = std::move(current_hour.out);
      last_hour.filename = std::move(current_hour.filename);
      // Open a new log for the current hour.
      open_log_file(current_hour, unix_ts);
      // Don't forget to log our event!
      current_hour.out.write(buf.data(), buf.size());
    } else {
      // And more than an hour in the future. Close our logs, open a log for the
      // current hour, and return to the one-logfile state.
      last_hour.out.close();
      current_hour.out.close();
      open_log_file(current_hour, unix_ts);
      // Don't forget to log the event!
      current_hour.out.write(buf.data(), buf.size());
      // Calculate data for last hour.
      auto current_hour_ts = align_to_hour(unix_ts);
      last_hour.start = current_hour_ts - secs_per_hour;
      last_hour.end = current_hour_ts;
      self->become(running_one_log(self));
    }
  } else {
    // MUST be older than last hour.
    aout(self) << "ERR unexpected timestamp in write two logs: " << unix_ts
               << " doesn't fit into [" << last_hour.start << ", "
               << last_hour.end << "), [" << current_hour.start << ", "
               << current_hour.end << ")" << std::endl;
  }
}

// -- running behaviors -------------------------------------------------------

caf::behavior collector_idle(caf::stateful_actor<collector_state>* self) {
  self->set_default_handler(caf::print_and_drop);
  self->set_down_handler([=](caf::down_msg&) { self->quit(); });
  self->state.self = self;
  return {
    [=](done_atom) { self->quit(); },
    /*
    [=](const json& j, std::time_t unix_ts) {
      self->monitor(self->current_sender());
      auto& s = self->state;
      auto current_hour_ts = align_to_hour(unix_ts);
      // Open file for this hour. Let's just assume it's the current hour.
      auto fn = s.generate_file_name(unix_ts);
      s.current_hour.out.open(fn, std::fstream::out | std::fstream::app);
      s.current_hour.ts = current_hour_ts;
      s.current_hour.filename = std::move(fn);
      s.current_hour.out << j << std::endl;
      // Calculate data for last and next hour.
      s.last_hour.ts = current_hour_ts - secs_per_hour;
      s.next_hour_start = current_hour_ts + secs_per_hour;
      self->become(caf::keep_behavior, running_one_log(self));
    },
    */
    [=](std::vector<char>& buf, std::time_t unix_ts) {
      self->monitor(self->current_sender());
      self->state.buffered_write_idle(buf, unix_ts);
      return std::move(buf);
    },
    [=](const scamper::reply& rep, std::time_t unix_ts) {
      self->monitor(self->current_sender());
      self->state.write_idle(to_log_line(rep), unix_ts);
    },
    [=](const packet& pkt, std::time_t unix_ts) {
      auto ev = json{{"trigger", pkt}, {"reaction", nullptr}};
      self->state.write_idle(to_string(ev), unix_ts);
    },
    [=](const packet& pkt, const probe::request& req, std::time_t unix_ts) {
      auto ev = json{{"trigger", pkt}, {"reaction", req}};
      self->state.write_idle(to_string(ev), unix_ts);
    },
    [=](const task& tsk, std::time_t unix_ts) {
      auto ev = json{tsk};
      self->state.write_idle(to_string(ev), unix_ts);
    },
    // For testing.
    [=](flush_atom) {
      auto& s = self->state;
      if (s.current_hour.out.is_open()) {
        s.current_hour.out.flush();
      }
      if (s.last_hour.out.is_open()) {
        s.last_hour.out.flush();
      }
    },
    [=](get_atom) { return std::string{"idle"}; },
  };
}

caf::behavior running_one_log(caf::stateful_actor<collector_state>* self) {
  return {
    [=](spoki::done_atom) {
      auto& s = self->state;
      if (s.current_hour.out.is_open())
        s.current_hour.out.close();
      if (s.last_hour.out.is_open())
        s.last_hour.out.close();
      self->quit();
    },
    [=](std::vector<char>& buf, std::time_t unix_ts) {
      self->state.buffered_write_one_log(buf, unix_ts);
      return std::move(buf);
    },
    [=](const scamper::reply& rep, std::time_t unix_ts) {
      self->state.write_one_log(to_log_line(rep), unix_ts);
    },
    [=](const packet& pkt, std::time_t unix_ts) {
      auto ev = json{{"trigger", pkt}, {"reaction", nullptr}};
      self->state.write_one_log(to_string(ev), unix_ts);
    },
    [=](const packet& pkt, const probe::request& req, std::time_t unix_ts) {
      auto ev = json{{"trigger", pkt}, {"reaction", req}};
      self->state.write_one_log(to_string(ev), unix_ts);
    },
    [=](const task& tsk, std::time_t unix_ts) {
      auto ev = json{tsk};
      self->state.write_one_log(to_string(ev), unix_ts);
    },
    // For testing.
    [=](flush_atom) {
      auto& s = self->state;
      if (s.current_hour.out.is_open()) {
        s.current_hour.out.flush();
      }
      if (s.last_hour.out.is_open()) {
        s.last_hour.out.flush();
      }
    },
    [=](get_atom) { return std::string{"one log"}; },
  };
}

caf::behavior running_two_logs(caf::stateful_actor<collector_state>* self) {
  return {
    [=](spoki::done_atom) {
      auto& s = self->state;
      if (s.current_hour.out.is_open())
        s.current_hour.out.close();
      if (s.last_hour.out.is_open())
        s.last_hour.out.close();
      self->quit();
    },
    [=](std::vector<char>& buf, std::time_t unix_ts) {
      self->state.buffered_write_two_logs(buf, unix_ts);
      return std::move(buf);
    },
    [=](const scamper::reply& rep, std::time_t unix_ts) {
      self->state.write_two_logs(to_log_line(rep), unix_ts);
    },
    [=](const packet& pkt, std::time_t unix_ts) {
      auto ev = json{{"trigger", pkt}, {"reaction", nullptr}};
      self->state.write_two_logs(to_string(ev), unix_ts);
    },
    [=](const packet& pkt, const probe::request& req, std::time_t unix_ts) {
      auto ev = json{{"trigger", pkt}, {"reaction", req}};
      self->state.write_two_logs(to_string(ev), unix_ts);
    },
    [=](const task& tsk, std::time_t unix_ts) {
      auto ev = json{tsk};
      self->state.write_two_logs(to_string(ev), unix_ts);
    },
    // For testing.
    [=](flush_atom) {
      auto& s = self->state;
      if (s.current_hour.out.is_open()) {
        s.current_hour.out.flush();
      }
      if (s.last_hour.out.is_open()) {
        s.last_hour.out.flush();
      }
    },
    [=](get_atom) { return std::string{"two logs"}; },
  };
}

} // namespace spoki
