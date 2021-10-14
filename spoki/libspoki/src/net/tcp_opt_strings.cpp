// clang-format off
// DO NOT EDIT: this file is auto-generated by generate-enum-strings.
// Run the target update-enum-strings if this file is out of sync.
#include "spoki/config.hpp"
#include "caf/string_view.hpp"

SPOKI_PUSH_DEPRECATED_WARNING

#include "spoki/net/tcp_opt.hpp"

#include <string>

namespace spoki::net {

std::string to_string(tcp_opt x) {
  switch(x) {
    default:
      return "???";
    case tcp_opt::end_of_list:
      return "spoki::net::tcp_opt::end_of_list";
    case tcp_opt::noop:
      return "spoki::net::tcp_opt::noop";
    case tcp_opt::mss:
      return "spoki::net::tcp_opt::mss";
    case tcp_opt::window_scale:
      return "spoki::net::tcp_opt::window_scale";
    case tcp_opt::sack_permitted:
      return "spoki::net::tcp_opt::sack_permitted";
    case tcp_opt::sack:
      return "spoki::net::tcp_opt::sack";
    case tcp_opt::timestamp:
      return "spoki::net::tcp_opt::timestamp";
    case tcp_opt::other:
      return "spoki::net::tcp_opt::other";
  };
}

bool from_string(caf::string_view in, tcp_opt& out) {
  if (in == "spoki::net::tcp_opt::end_of_list") {
    out = tcp_opt::end_of_list;
    return true;
  } else if (in == "spoki::net::tcp_opt::noop") {
    out = tcp_opt::noop;
    return true;
  } else if (in == "spoki::net::tcp_opt::mss") {
    out = tcp_opt::mss;
    return true;
  } else if (in == "spoki::net::tcp_opt::window_scale") {
    out = tcp_opt::window_scale;
    return true;
  } else if (in == "spoki::net::tcp_opt::sack_permitted") {
    out = tcp_opt::sack_permitted;
    return true;
  } else if (in == "spoki::net::tcp_opt::sack") {
    out = tcp_opt::sack;
    return true;
  } else if (in == "spoki::net::tcp_opt::timestamp") {
    out = tcp_opt::timestamp;
    return true;
  } else if (in == "spoki::net::tcp_opt::other") {
    out = tcp_opt::other;
    return true;
  } else {
    return false;
  }
}

bool from_integer(std::underlying_type_t<tcp_opt> in,
                  tcp_opt& out) {
  auto result = static_cast<tcp_opt>(in);
  switch(result) {
    default:
      return false;
    case tcp_opt::end_of_list:
    case tcp_opt::noop:
    case tcp_opt::mss:
    case tcp_opt::window_scale:
    case tcp_opt::sack_permitted:
    case tcp_opt::sack:
    case tcp_opt::timestamp:
    case tcp_opt::other:
      out = result;
      return true;
  };
}

} // namespace spoki::net

SPOKI_POP_WARNINGS
