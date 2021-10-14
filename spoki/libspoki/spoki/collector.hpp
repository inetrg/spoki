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

#pragma once

#include <fstream>

#include <caf/all.hpp>

#include "spoki/detail/core_export.hpp"
#include "spoki/fwd.hpp"

namespace spoki {

// -- CSV log lines ------------------------------------------------------------

/// Creates entries for the scamper reply log files.
SPOKI_CORE_EXPORT void append_log_entry(std::vector<char>& buf,
                                        const scamper::reply& repl);

/// Creates entries for the raw logs.
SPOKI_CORE_EXPORT void append_log_entry(std::vector<char>& buf,
                                        const packet& pkt);
SPOKI_CORE_EXPORT void append_log_entry(std::vector<char>& buf,
                                        const packet& pkt,
                                        const probe::request& req);

// -- collector ----------------------------------------------------------------

struct out_file {
  std::time_t start;
  std::time_t end;
  std::ofstream out;
  std::string filename;
};

/// State held by a result collector which writes incoming data into a log file,
/// rotating logs in a given interval.
struct SPOKI_CORE_EXPORT collector_state {
  // -- some file stuff --------------------------------------------------------

  /// Generate the filename for an event with `unix_ts`. The filename will be
  /// aligned to the full hour of the timestamp. This function takes the state
  /// of the logger into account, i.e., its directory, datasource, protocol,
  /// component, and the unix timestamp.
  std::string generate_file_name(std::time_t unix_ts);

  /// Open a new file on a given `out_file` with the timestamp `unix_tx`. This
  /// writes the header to the file if it did not exist priorly. Otherwise it
  /// opens the file to append more data.
  void open_log_file(out_file& of, time_t unix_ts);

  // -- writing ----------------------------------------------------------------

  void write_idle(std::string, std::time_t unix_ts);

  void write_one_log(std::string, std::time_t unix_ts);

  void write_two_logs(std::string, std::time_t unix_ts);

  // -- buffered writing -------------------------------------------------------

  void buffered_write_idle(const std::vector<char>& buf, std::time_t unix_ts);

  void buffered_write_one_log(const std::vector<char>& buf,
                              std::time_t unix_ts);

  void buffered_write_two_logs(const std::vector<char>& buf,
                               std::time_t unix_ts);

  // -- state ------------------------------------------------------------------

  // Outfile for the current hour.
  out_file current_hour;

  // Outfile for the last hour.
  out_file last_hour;

  /// Directory to write the logs to.
  std::string dir;

  /// Log rotation interval defaults to 60 minutes.
  std::chrono::seconds interval;

  /// Tag that specifies the datasource for log file names.
  std::string datasource_tag;

  /// Tag that specifices the protocol.
  std::string protocol_tag;

  /// An id included in the filename, should be unique per collector.
  std::string component_tag;

  /// Header for the CSV file.
  std::string header;

  /// Actor name that appears in logs (collector).
  static const char* name;

  /// Collector id to distinguish log files of different shards.
  uint32_t id;

  /// Our self reference.
  caf::stateful_actor<collector_state>* self;
};

/// Returns the behavior for a collector writing logs to `dir`. Each log
/// includes `id` in ints name and starts with `header`.
SPOKI_CORE_EXPORT caf::behavior
collector(caf::stateful_actor<collector_state>* self, std::string dir,
          std::string component, std::string protocol, std::string header,
          uint32_t collector_id);

} // namespace spoki
