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

#define SUITE collector
#include "test.hpp"

#include <cstdio>
#include <optional>
#include <random>

#include <glob.h>

#include <caf/test/bdd_dsl.hpp>

#include "spoki/atoms.hpp"
#include "spoki/collector.hpp"

namespace {

constexpr uint32_t seconds_per_hour = 3600;

constexpr std::string_view m1 = "message one\n";
constexpr std::string_view m2 = "message two\n";
constexpr std::string_view m3 = "message three\n";
constexpr std::string_view m4 = "message four\n";
constexpr std::string_view m5 = "message five\n";
constexpr std::string_view m6 = "message six\n";
constexpr std::string_view m7 = "message seven\n";
constexpr std::string_view m8 = "message eight\n";
constexpr std::string_view m9 = "message nine\n";

// Found on StackOverflow: https://stackoverflow.com/a/24703135
std::vector<std::string> glob_files(const std::string& pattern) {
  std::vector<std::string> files;
  glob_t glob_result;
  auto rc = glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
  if (rc != 0) {
    // TODO: ...?
    return files;
  }
  for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
    files.push_back(std::string(glob_result.gl_pathv[i]));
  }
  globfree(&glob_result);
  return files;
}

uint64_t generate_id() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint64_t> distrib;
  return distrib(gen);
}

std::vector<char> vectorify(std::string_view sv) {
  std::vector<char> buf;
  buf.reserve(sv.size());
  buf.insert(buf.begin(), sv.begin(), sv.end());
  return buf;
}

// From SO: https://stackoverflow.com/a/42844629
bool ends_with(std::string_view str, std::string_view suffix) {
  return str.size() >= suffix.size()
         && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

std::optional<std::string>
find_with_suffix(const std::vector<std::string>& haystack,
                 std::string_view needle) {
  for (const auto& elem : haystack) {
    if (ends_with(elem, needle)) {
      return elem;
    }
  }
  return {};
}

std::string read_file(const std::string& fn) {
  std::ifstream fh(fn);
  std::string str;

  fh.seekg(0, std::ios::end);
  str.reserve(fh.tellg());
  fh.seekg(0, std::ios::beg);

  str.assign((std::istreambuf_iterator<char>(fh)),
             (std::istreambuf_iterator<char>()));
  return str;
}

// -- fixture ------------------------------------------------------------------

struct fixture : test_coordinator_fixture<> {
  fixture() : self(sys, true) {
    protocol = std::to_string(generate_id());
    collector = sys.spawn(spoki::collector, directory, component, protocol,
                          header, 0);
    run();
  }

  ~fixture() {
    remove_log_files();
  }

  time_t next_ts() {
    return ts++;
  }

  time_t next_ts(uint32_t add_me) {
    ts += add_me;
    return ts;
  }

  std::vector<std::string> list_log_files() {
    auto serach_string = "*" + protocol + "*";
    // auto serach_string = "*collector-test*";
    return glob_files(serach_string);
  }

  void remove_log_files() {
    auto matching_files = list_log_files();
    for (auto& fn : matching_files) {
      std::remove(fn.c_str());
    }
  }

  void collect(std::string_view str, time_t ts) {
    auto vec = vectorify(str);
    inject((std::vector<char>, time_t), from(self).to(collector).with(vec, ts));
    expect((std::vector<char>), from(collector).to(self).with(vec));
  }

  void flush() {
    self->send(collector, spoki::flush_atom_v);
    expect((spoki::flush_atom),
           from(self).to(collector).with(spoki::flush_atom_v));
    // run();
  }

  caf::scoped_actor self;
  caf::actor collector;

  const std::string directory = ".";
  const std::string component = "collector-test";
  const std::string header = "this header is intentionally left blank\n";
  std::string protocol;

  time_t ts = 0;
};

} // namespace

// -- tests --------------------------------------------------------------------

FIXTURE_SCOPE(collector_tests, fixture)

SCENARIO("collector starts in idle state") {
  GIVEN("a collector") {
    WHEN("nothing happend") {
      THEN("collectr is in idle state") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("idle"));
      }
    }
  }
}

SCENARIO("events span two consecutive hours") {
  using namespace std::string_literals;
  GIVEN("a collector") {
    WHEN("sending events from the same hour") {
      collect(m1, next_ts());
      collect(m2, next_ts());
      collect(m3, next_ts());
      flush();
      auto files = list_log_files();
      THEN("one log file is created") {
        CHECK(files.size() == 1);
      }
      THEN("the file contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'one log'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("one log"));
      }
    }
    WHEN("sending events from the next hour") {
      collect(m4, next_ts() + seconds_per_hour);
      collect(m5, next_ts() + seconds_per_hour);
      collect(m6, next_ts() + seconds_per_hour);
      flush();
      auto files = list_log_files();
      THEN("two log files are present") {
        CHECK(files.size() == 2);
      }
      THEN("each log contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.3600.0.csv");
        CHECK(fn);
        expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'two logs'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("two logs"));
      }
    }
  }
}

SCENARIO("events span two consecutive hours, opposite direction") {
  using namespace std::string_literals;
  GIVEN("a collector") {
    WHEN("sending events from the same hour") {
      collect(m4, next_ts() + seconds_per_hour);
      collect(m5, next_ts() + seconds_per_hour);
      collect(m6, next_ts() + seconds_per_hour);
      flush();
      auto files = list_log_files();
      THEN("one log file is created") {
        CHECK(files.size() == 1);
      }
      THEN("the file contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.3600.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        auto content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'one log'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("one log"));
      }
    }
    WHEN("sending events from the previous hour") {
      collect(m1, next_ts());
      collect(m2, next_ts());
      collect(m3, next_ts());
      flush();
      auto files = list_log_files();
      THEN("two log files are present") {
        CHECK(files.size() == 2);
      }
      THEN("each log contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.3600.0.csv");
        CHECK(fn);
        expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'two logs'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("two logs"));
      }
    }
  }
}

SCENARIO("events span three consecutive hours") {
  using namespace std::string_literals;
  GIVEN("a collector") {
    WHEN("sending events from the first hour") {
      collect(m1, next_ts());
      collect(m2, next_ts());
      collect(m3, next_ts());
      flush();
      auto files = list_log_files();
      THEN("one log file is created") {
        CHECK(files.size() == 1);
      }
      THEN("the file contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'one log'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("one log"));
      }
    }
    WHEN("sending events from the second hour") {
      collect(m4, next_ts() + seconds_per_hour);
      collect(m5, next_ts() + seconds_per_hour);
      collect(m6, next_ts() + seconds_per_hour);
      flush();
      auto files = list_log_files();
      THEN("two log files are present") {
        CHECK(files.size() == 2);
      }
      THEN("each log contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.3600.0.csv");
        CHECK(fn);
        expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'two logs'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("two logs"));
      }
    }
    WHEN("sending events from the third hour") {
      collect(m7, next_ts() + (2 * seconds_per_hour));
      collect(m8, next_ts() + (2 * seconds_per_hour));
      collect(m9, next_ts() + (2 * seconds_per_hour));
      flush();
      auto files = list_log_files();
      THEN("two log files are present") {
        CHECK(files.size() == 3);
      }
      THEN("each log contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.3600.0.csv");
        CHECK(fn);
        expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.7200.0.csv");
        CHECK(fn);
        expected = header;
        expected += m7;
        expected += m8;
        expected += m9;
        content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'two logs'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("two logs"));
      }
    }
    WHEN("sending events from the second hour again") {
      collect(m4, next_ts() + seconds_per_hour);
      collect(m5, next_ts() + seconds_per_hour);
      collect(m6, next_ts() + seconds_per_hour);
      flush();
      auto files = list_log_files();
      THEN("two log files are present") {
        CHECK(files.size() == 3);
      }
      THEN("each log contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.3600.0.csv");
        CHECK(fn);
        expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        expected += m4;
        expected += m5;
        expected += m6;
        content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.7200.0.csv");
        CHECK(fn);
        expected = header;
        expected += m7;
        expected += m8;
        expected += m9;
        content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'two logs'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("two logs"));
      }
    }
  }
}

SCENARIO("events span skip an hour") {
  using namespace std::string_literals;
  GIVEN("a collector") {
    WHEN("sending events from the first hour") {
      collect(m1, next_ts());
      collect(m2, next_ts());
      collect(m3, next_ts());
      flush();
      auto files = list_log_files();
      THEN("one log file is created") {
        CHECK(files.size() == 1);
      }
      THEN("the file contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'one log'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("one log"));
      }
    }
    WHEN("sending events from the second hour") {
      collect(m4, next_ts() + seconds_per_hour);
      collect(m5, next_ts() + seconds_per_hour);
      collect(m6, next_ts() + seconds_per_hour);
      flush();
      auto files = list_log_files();
      THEN("two log files are present") {
        CHECK(files.size() == 2);
      }
      THEN("each log contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.3600.0.csv");
        CHECK(fn);
        expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'two logs'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("two logs"));
      }
    }
    WHEN("sending events from the third hour") {
      collect(m7, next_ts() + (3 * seconds_per_hour));
      collect(m8, next_ts() + (3 * seconds_per_hour));
      collect(m9, next_ts() + (3 * seconds_per_hour));
      flush();
      auto files = list_log_files();
      THEN("two log files are present") {
        CHECK(files.size() == 3);
      }
      THEN("each log contains the expected lines") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.3600.0.csv");
        CHECK(fn);
        expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.10800.0.csv");
        CHECK(fn);
        expected = header;
        expected += m7;
        expected += m8;
        expected += m9;
        content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'one log'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("one log"));
      }
    }
    WHEN("sending events from the skipped hour") {
      collect(m4, next_ts() + (2 * seconds_per_hour));
      collect(m5, next_ts() + (2 * seconds_per_hour));
      collect(m6, next_ts() + (2 * seconds_per_hour));
      flush();
      auto files = list_log_files();
      THEN("two log files are present") {
        CHECK(files.size() == 4);
      }
      THEN("the new log contains the expected events") {
        auto fn = find_with_suffix(files, ".collector-test.7200.0.csv");
        CHECK(fn);
        auto expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        auto content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'two logs'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("two logs"));
      }
    }
  }
}

SCENARIO("events don't arrive in order") {
  GIVEN("a collector") {
    WHEN("mixing events from different consecutive hours.") {
      collect(m1, next_ts());
      collect(m4, next_ts() + seconds_per_hour);
      collect(m2, next_ts());
      collect(m5, next_ts() + seconds_per_hour);
      collect(m3, next_ts());
      collect(m6, next_ts() + seconds_per_hour);
      flush();
      auto files = list_log_files();
      THEN("two logfiles were created") {
        CHECK(files.size() == 2);
      }
      THEN("each log contains the expected lined") {
        auto fn = find_with_suffix(files, ".collector-test.0.0.csv");
        CHECK(fn);
        std::string expected = header;
        expected += m1;
        expected += m2;
        expected += m3;
        auto content = read_file(*fn);
        CHECK(expected == content);
        fn = find_with_suffix(files, ".collector-test.3600.0.csv");
        CHECK(fn);
        expected = header;
        expected += m4;
        expected += m5;
        expected += m6;
        content = read_file(*fn);
        CHECK(expected == content);
      }
      THEN("the collector is in state 'two logs'") {
        inject((spoki::get_atom),
               from(self).to(collector).with(spoki::get_atom_v));
        expect((std::string), from(collector).to(self).with("two logs"));
      }
    }
  }
}

FIXTURE_SCOPE_END()