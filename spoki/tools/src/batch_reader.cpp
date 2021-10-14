
#include "batch_reader.hpp"

#include <functional>

#include <caf/all.hpp>
#include <caf/ipv4_address.hpp>
#include <caf/string_algorithms.hpp>

#include "spoki/atoms.hpp"

#include "udp_processor.hpp"

namespace {

void down_handler(caf::scheduled_actor* ptr, caf::down_msg& msg) {
  aout(ptr) << ptr->name() << "got down message from " << to_string(msg.source)
            << " with reason " << caf::to_string(msg.reason) << std::endl;
}

void exit_handler(caf::scheduled_actor* ptr, caf::exit_msg& msg) {
  aout(ptr) << ptr->name() << "got exit message from " << to_string(msg.source)
            << " with reason " << caf::to_string(msg.reason) << std::endl;
}

} // namespace

const char* brs::name = "batch_reader";

caf::behavior batch_reader(caf::stateful_actor<brs>* self,
                           const std::string& name) {
  self->set_down_handler(down_handler);
  self->set_exit_handler(exit_handler);
  auto& s = self->state;
  s.in.open(name);
  if (!s.in.is_open()) {
    return {};
  }
  return {
    [=](spoki::request_atom, uint32_t num) {
      caf::actor dst = caf::actor_cast<caf::actor>(self->current_sender());
      std::vector<up::endpoint> batch;
      batch.reserve(num);
      uint32_t collected = 0;
      std::string buf;
      while (collected < num && getline(self->state.in, buf)) {
        caf::string_view line{buf};
        auto none_ws = line.find_first_not_of(' ');
        line.remove_prefix(none_ws);
        if (caf::starts_with(line, "#"))
          continue;
        std::vector<std::string> splits;
        caf::split(splits, line, ",");
        // expecting: src ip, dst ip, src port, dst port, pl
        if (splits.size() != 5) {
          aout(self) << "Line not well formated: '" << line << "'" << std::endl;
          continue;
        }
        batch.emplace_back();
        auto& ep = batch.back();
        auto res = caf::parse(splits[0], ep.saddr);
        if (res) {
          aout(self) << "Could not parse addr form '" << splits[0] << "'"
                     << std::endl;
          batch.pop_back();
          continue;
        }
        res = caf::parse(splits[1], ep.daddr);
        if (res) {
          aout(self) << "Could not parse addr form '" << splits[0] << "'"
                     << std::endl;
          batch.pop_back();
          continue;
        }
        ep.sport = static_cast<uint16_t>(stol(splits[2]));
        ep.dport = static_cast<uint16_t>(stol(splits[3]));
        std::copy(splits[4].begin(), splits[4].end(),
                  std::back_inserter(ep.payload));
        ++collected;
      }
      self->send(dst, std::move(batch));
    },
    [=](spoki::done_atom) {
      self->state.in.close();
      self->quit();
    },
  };
}
