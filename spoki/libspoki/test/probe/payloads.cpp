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

#define SUITE probe.payloads
#include "test.hpp"

#include <algorithm>

#include "spoki/probe/payloads.hpp"

namespace {

#include <stdexcept>

std::vector<char> hex_to_chars(const std::string& input) {
  static const char* const lut = "0123456789abcdef";
  size_t len = input.length();
  if (len & 1)
    FAIL("wrong length");

  std::vector<char> output;
  output.reserve(len / 2);
  for (size_t i = 0; i < len; i += 2) {
    char a = input[i];
    std::cout << "val: " << a << std::endl;
    const char* p = std::lower_bound(lut, lut + 16, a);
    if (*p != a)
      throw std::invalid_argument("not a hex digit");

    char b = input[i + 1];
    const char* q = std::lower_bound(lut, lut + 16, b);
    if (*q != b)
      throw std::invalid_argument("not a hex digit");

    output.push_back(((p - lut) << 4) | (q - lut));
  }
  return output;
}

} // namespace

TEST(check string conversion) {
  auto vecs = spoki::probe::get_payloads();
  auto strs = spoki::probe::get_payload_hex_strs();
  for (auto p : strs) {
    auto port = p.first;
    auto& str = p.second;
    REQUIRE(vecs.count(port) > 0);
    auto expected = vecs[port];
    std::vector<char> recreated = hex_to_chars(str);
    CHECK_EQUAL(expected, recreated);
  }
}
