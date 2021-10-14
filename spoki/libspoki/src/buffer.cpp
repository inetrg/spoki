/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2019
 * Authors: Raphael Hiesgen
 *
 * All rights reserved.
 *
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#include "spoki/buffer.hpp"
#include "spoki/atoms.hpp"
#include "spoki/collector.hpp"
#include "spoki/packet.hpp"
#include "spoki/probe/request.hpp"
#include "spoki/scamper/reply.hpp"

namespace spoki {

namespace {

inline std::time_t align_to_hour(std::time_t ts) {
  return (ts - (ts % secs_per_hour));
}

} // namespace

caf::behavior buffer_default(caf::stateful_actor<buffer_state>* self,
                             caf::actor collector) {
  return buffer(self, collector, spoki::buffer_reserve_mem,
                spoki::buffer_send_mem);
  ;
}

caf::behavior buffer(caf::stateful_actor<buffer_state>* self,
                     caf::actor collector, size_t reserve_size,
                     size_t write_threshold) {
  self->state.reserve_size = reserve_size;
  self->state.write_threshold = write_threshold;
  self->state.collector = collector;
  self->state.buffer.reserve(self->state.reserve_size);
  self->set_default_handler(caf::print_and_drop);

  return {
    [=](const spoki::packet& pkt) {
      auto unix_ts = pkt.unix_ts();
      auto& s = self->state;
      auto aligned = align_to_hour(unix_ts);
      if (s.buffer.empty()) {
        s.unix_ts = aligned;
      } else if (aligned != s.unix_ts) {
        // aout(self) << "[" << self->id() << "] different timestamp\n";
        self->send(s.collector, std::move(s.buffer), s.unix_ts);
        if (s.got_backup_buffer) {
          // aout(self) << "[" << self->id() << "] > reusing buffer\n";
          std::swap(s.buffer, s.next_buffer);
          s.got_backup_buffer = false;
        } else {
          aout(self) << "[" << self->id() << "] > allocating new buffer\n";
          s.buffer.clear();
          s.buffer.reserve(s.reserve_size);
        }
        s.unix_ts = aligned;
      }
      spoki::append_log_entry(s.buffer, pkt);
      s.buffer.push_back('\n');
      if (s.buffer.size() > s.write_threshold) {
        // aout(self) << "[" << self->id() << "] hit threshold\n";
        self->send(s.collector, std::move(s.buffer), s.unix_ts);
        if (s.got_backup_buffer) {
          // aout(self) << "[" << self->id() << "] > reusing buffer\n";
          std::swap(s.buffer, s.next_buffer);
          s.got_backup_buffer = false;
        } else {
          aout(self) << "[" << self->id() << "] > allocating new buffer\n";
          s.buffer.clear();
          s.buffer.reserve(s.reserve_size);
        }
      }
    },

    [=](const spoki::packet& pkt, const spoki::probe::request& req) {
      auto unix_ts = pkt.unix_ts();
      auto& s = self->state;
      auto aligned = align_to_hour(unix_ts);
      if (s.buffer.empty()) {
        s.unix_ts = aligned;
      } else if (aligned != s.unix_ts) {
        // aout(self) << "[" << self->id() << "] different timestamp\n";
        self->send(s.collector, std::move(s.buffer), s.unix_ts);
        if (s.got_backup_buffer) {
          // aout(self) << "[" << self->id() << "] > reusing buffer\n";
          std::swap(s.buffer, s.next_buffer);
          s.got_backup_buffer = false;
        } else {
          aout(self) << "[" << self->id() << "] > allocating new buffer\n";
          s.buffer.clear();
          s.buffer.reserve(s.reserve_size);
        }
        s.unix_ts = aligned;
      }
      spoki::append_log_entry(s.buffer, pkt, req);
      s.buffer.push_back('\n');
      if (s.buffer.size() > s.write_threshold) {
        // aout(self) << "[" << self->id() << "] hit threshold\n";
        self->send(s.collector, std::move(s.buffer), s.unix_ts);
        if (s.got_backup_buffer) {
          // aout(self) << "[" << self->id() << "] > reusing buffer\n";
          std::swap(s.buffer, s.next_buffer);
          s.got_backup_buffer = false;
        } else {
          aout(self) << "[" << self->id() << "] > allocating new buffer\n";
          s.buffer.clear();
          s.buffer.reserve(s.reserve_size);
        }
      }
    },

    [=](const scamper::reply& rep) {
      using namespace std::chrono;
      auto start = seconds(rep.start.sec) + microseconds(rep.start.usec);
      auto ts = duration_cast<seconds>(start).count();
      auto unix_ts = static_cast<std::time_t>(ts);
      auto& s = self->state;
      auto aligned = align_to_hour(unix_ts);
      if (s.buffer.empty()) {
        s.unix_ts = aligned;
      } else if (aligned != s.unix_ts) {
        // aout(self) << "[" << self->id() << "] different timestamp\n";
        self->send(s.collector, std::move(s.buffer), s.unix_ts);
        if (s.got_backup_buffer) {
          // aout(self) << "[" << self->id() << "] > reusing buffer\n";
          std::swap(s.buffer, s.next_buffer);
          s.got_backup_buffer = false;
        } else {
          aout(self) << "[" << self->id() << "] > allocating new buffer\n";
          s.buffer.clear();
          s.buffer.reserve(s.reserve_size);
        }
        s.unix_ts = aligned;
      }
      spoki::append_log_entry(s.buffer, rep);
      s.buffer.push_back('\n');
      if (s.buffer.size() > s.write_threshold) {
        // aout(self) << "[" << self->id() << "] hit threshold\n";
        self->send(s.collector, std::move(s.buffer), s.unix_ts);
        if (s.got_backup_buffer) {
          // aout(self) << "[" << self->id() << "] > reusing buffer\n";
          std::swap(s.buffer, s.next_buffer);
          s.got_backup_buffer = false;
        } else {
          aout(self) << "[" << self->id() << "] > allocating new buffer\n";
          s.buffer.clear();
          s.buffer.reserve(s.reserve_size);
        }
      }
    },

    [=](std::vector<char>& buf) {
      std::swap(self->state.next_buffer, buf);
      self->state.next_buffer.clear();
      self->state.next_buffer.reserve(self->state.reserve_size);
      self->state.got_backup_buffer = true;
    },
  };
}

} // namespace spoki
