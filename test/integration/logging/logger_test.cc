/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <adios2/cxx11/ADIOS.h>
#include <cassert>
#include "coeus/MetadataSerializer.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace hapi = hermes::api;

int main() {

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::warn);
  console_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/engine_test.txt", true);
  file_sink->set_level(spdlog::level::trace);
  file_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  auto mix_log = std::make_shared<spdlog::logger>(spdlog::logger("debug_logger", {console_sink, file_sink}));
  mix_log->set_level(spdlog::level::debug);

  SPDLOG_LOGGER_WARN(mix_log, "this message should appear in both console and file");

  SPDLOG_LOGGER_WARN(mix_log, "test number {}", 42);
  SPDLOG_LOGGER_WARN(mix_log, "test string {}", "hello world");
  SPDLOG_LOGGER_WARN(mix_log, "test multiple {} {}", "hello world", 42);

  SPDLOG_LOGGER_ERROR(mix_log, "test multiple {} {}", "hello world", 42);
  SPDLOG_LOGGER_CRITICAL(mix_log, "test multiple {} {}", "hello world", 42);

  SPDLOG_LOGGER_INFO(mix_log, "Only to file {} {}", "hello world", 42);

  return 0;
}
