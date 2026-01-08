/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_LOGGING_H_
#define HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_LOGGING_H_

#include <climits>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include "formatter.h"
#include "hermes_shm/introspect/system_info.h"

/**
 * Log level codes for filtering messages at compile-time and runtime
 * Lower values = more verbose, higher values = less verbose
 * Defined as macros for global accessibility without namespace qualification
 */
#ifndef kDebug
#define kDebug 0    /**< Low-priority debugging information */
#endif
#ifndef kInfo
#define kInfo 1     /**< Useful information the user should know */
#endif
#ifndef kWarning
#define kWarning 2  /**< Something might be wrong */
#endif
#ifndef kError
#define kError 3    /**< A non-fatal error has occurred */
#endif
#ifndef kFatal
#define kFatal 4    /**< A fatal error has occurred */
#endif

/**
 * Compile-time log level threshold
 * Messages below this level will be compiled out entirely
 * Default: kInfo (1) - debug messages excluded in release builds
 */
#ifndef HSHM_LOG_LEVEL
#define HSHM_LOG_LEVEL kInfo
#endif

namespace hshm {

/** Simplify access to Logger singleton */
#define HSHM_LOG hshm::CrossSingleton<hshm::Logger>::GetInstance()

/**
 * Hermes Print. Like printf, except types are inferred
 */
#define HIPRINT(...) HSHM_LOG->Print(__VA_ARGS__)

/**
 * Hermes SHM Log - Unified logging macro
 *
 * Messages with LOG_CODE < HSHM_LOG_LEVEL are compiled out entirely.
 * Messages with LOG_CODE >= HSHM_LOG_LEVEL are subject to runtime filtering
 * via the HSHM_LOG_LEVEL environment variable.
 *
 * @param LOG_CODE The log level (kDebug, kInfo, kWarning, kError, kFatal)
 * @param ... Format string and arguments
 */
#define HLOG(LOG_CODE, ...)                                               \
  do {                                                                    \
    if constexpr (LOG_CODE >= HSHM_LOG_LEVEL) {                           \
      HSHM_LOG->Log<LOG_CODE>(__FILE__, __func__, __LINE__, __VA_ARGS__); \
    }                                                                     \
  } while (false)

/**
 * Logger class for handling log output
 *
 * Supports:
 * - Runtime log level filtering via HSHM_LOG_LEVEL environment variable
 * - File output via HSHM_LOG_OUT environment variable
 * - Routing to stdout (debug/info) or stderr (warning/error/fatal)
 */
class Logger {
 public:
  FILE *fout_;
  int runtime_log_level_;  /**< Runtime log level threshold */

  HSHM_CROSS_FUN
  Logger() {
#if HSHM_IS_HOST
    fout_ = nullptr;
    runtime_log_level_ = HSHM_LOG_LEVEL;  // Default to compile-time level

    // Check for runtime log level override
    std::string level_env = hshm::SystemInfo::Getenv(
        "HSHM_LOG_LEVEL", hshm::Unit<size_t>::Megabytes(1));
    if (!level_env.empty()) {
      // Parse log level - accept both numeric and string values
      if (level_env == "debug" || level_env == "DEBUG" || level_env == "0") {
        runtime_log_level_ = kDebug;
      } else if (level_env == "info" || level_env == "INFO" || level_env == "1") {
        runtime_log_level_ = kInfo;
      } else if (level_env == "warning" || level_env == "WARNING" || level_env == "2") {
        runtime_log_level_ = kWarning;
      } else if (level_env == "error" || level_env == "ERROR" || level_env == "3") {
        runtime_log_level_ = kError;
      } else if (level_env == "fatal" || level_env == "FATAL" || level_env == "4") {
        runtime_log_level_ = kFatal;
      } else {
        // Try to parse as integer
        try {
          runtime_log_level_ = std::stoi(level_env);
        } catch (...) {
          // Keep default on parse failure
        }
      }
    }

    // Check for file output
    std::string env = hshm::SystemInfo::Getenv(
        "HSHM_LOG_OUT", hshm::Unit<size_t>::Megabytes(1));
    if (!env.empty()) {
      fout_ = fopen(env.c_str(), "w");
    }
#endif
  }

  /**
   * Get the string representation of a log level
   * @param level The log level
   * @return String name of the log level
   */
  HSHM_CROSS_FUN
  static const char* GetLevelString(int level) {
    switch (level) {
      case kDebug: return "DEBUG";
      case kInfo: return "INFO";
      case kWarning: return "WARNING";
      case kError: return "ERROR";
      case kFatal: return "FATAL";
      default: return "UNKNOWN";
    }
  }

  /**
   * Check if a log level should be output based on runtime level
   * @param level The log level to check
   * @return true if the message should be logged
   */
  HSHM_CROSS_FUN
  bool ShouldLog(int level) const {
    return level >= runtime_log_level_;
  }

  template <typename... Args>
  HSHM_CROSS_FUN void Print(const char *fmt, Args &&...args) {
#if HSHM_IS_HOST
    std::string msg = hshm::Formatter::format(fmt, std::forward<Args>(args)...);
    std::string out = hshm::Formatter::format("{}\n", msg);
    std::cout << out;
    if (fout_) {
      fwrite(out.data(), 1, out.size(), fout_);
    }
#endif
  }

  template <int LOG_CODE, typename... Args>
  HSHM_CROSS_FUN void Log(const char *path, const char *func, int line,
                          const char *fmt, Args &&...args) {
#if HSHM_IS_HOST
    // Runtime log level check
    if (!ShouldLog(LOG_CODE)) {
      return;
    }

    const char* level = GetLevelString(LOG_CODE);
    std::string msg = hshm::Formatter::format(fmt, std::forward<Args>(args)...);
    int tid = SystemInfo::GetTid();
    std::string out = hshm::Formatter::format("{}:{} {} {} {} {}\n", path, line,
                                              level, tid, func, msg);

    // Route to appropriate output stream
    // Debug and Info go to stdout, Warning/Error/Fatal go to stderr
    if (LOG_CODE <= kInfo) {
      std::cout << out;
      fflush(stdout);
    } else {
      std::cerr << out;
      fflush(stderr);
    }

    // Also write to file if configured
    if (fout_) {
      fwrite(out.data(), 1, out.size(), fout_);
      fflush(fout_);
    }

    // Fatal errors terminate the program
    if (LOG_CODE == kFatal) {
      exit(1);
    }
#endif
  }
};

}  // namespace hshm

#endif  // HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_LOGGING_H_
