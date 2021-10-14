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

#include "spoki/time.hpp"

#include <chrono>

extern "C" {

#include <sys/time.h>
}

namespace spoki {

// Implementation from:
// https://stackoverflow.com/questions/39421089/convert-stdchronosystem-clocktime-point-to-struct-timeval-and-back#39421261

std::chrono::system_clock::time_point to_time_point(timeval tv) {
  using namespace std::chrono;
  return system_clock::time_point{seconds{tv.tv_sec}
                                  + microseconds{tv.tv_usec}};
}

std::chrono::system_clock::duration to_duration(timeval tv) {
  using namespace std::chrono;
  return system_clock::duration{seconds{tv.tv_sec} + microseconds{tv.tv_usec}};
}

/*
timeval to_timeval(std::chrono::system_clock::time_point tp) {
  using namespace std::chrono;
  auto s = time_point_cast<seconds>(tp);
  if (s > tp)
    s = s - seconds{1};
  auto us = duration_cast<microseconds>(tp - s);
  timeval tv;
  tv.tv_sec = s.time_since_epoch().count();
  tv.tv_usec = us.count();
  return tv;
}
*/

timeval to_timeval(timestamp tp) {
  using namespace std::chrono;
  auto s = time_point_cast<seconds>(tp);
  if (s > tp)
    s = s - seconds{1};
  auto us = duration_cast<microseconds>(tp - s);
  timeval tv;
  tv.tv_sec = s.time_since_epoch().count();
  tv.tv_usec = us.count();
  return tv;
}

} // namespace spoki
