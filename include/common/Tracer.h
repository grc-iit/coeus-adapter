//
// Created by jaime on 1/10/2024.
//

#ifndef DISTRIBUTEDTRACER_TRACER_H
#define DISTRIBUTEDTRACER_TRACER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <thread>
#include <string>
#include <sstream>
#include <unistd.h>

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>

std::string demangle(const char* name) {
  int status = -1;
  std::unique_ptr<char, void(*)(void*)> res{
      abi::__cxa_demangle(name, NULL, NULL, &status),
      std::free
  };
  return (status == 0) ? res.get() : name;
}
#else
std::string demangle(const char* name) {
    return name;
}
#endif

#ifdef debug_mode
#define TRACE_FUNC(...) TraceLogger traceLogger(__func__, ##__VA_ARGS__)
#else
#define TRACE_FUNC(...)
#endif

template<typename T>
class EasySingleton {
 protected:
  /** static instance. */
  static T* obj_;
  static std::mutex lock_;

 public:
  /**
   * Uses unique pointer to build a static global instance of variable.
   * @tparam T
   * @return instance of T
   */
  template<typename ...Args>
  static T* GetInstance(Args&& ...args) {
    if (obj_ == nullptr) {
      std::scoped_lock lock(lock_);
      if (obj_ == nullptr) {
        obj_ = new T(std::forward<Args>(args)...);
      }
    }
    return obj_;
  }
};
template <typename T>
T* EasySingleton<T>::obj_ = nullptr;
template <typename T>
std::mutex EasySingleton<T>::lock_ = std::mutex();

class TraceManager {
 public:
  explicit TraceManager(int rank) {
    init_(rank);
  }

  explicit TraceManager() {
    init_(getpid());
  }

  ~TraceManager() {
    auto log = spdlog::get("trace_logger");
    log->set_pattern(exitPattern);
    log->info("");
    spdlog::drop("trace_logger");
  }

 private:
  std::string entryPattern = {"["};
  std::string exitPattern = {"]"};
  std::string jsonPattern = {"{%v},"};

  void init_(int id){
    std::string fileName = "trace_" + std::to_string(id) + ".json";
    spdlog::basic_logger_mt("trace_logger", fileName);

    auto log = spdlog::get("trace_logger");
    log->set_pattern(entryPattern);
    log->info("");
    log->set_pattern(jsonPattern);
  }
};



class TraceLogger {
 public:
  template<typename... Args>
  explicit TraceLogger(std::string functionName, Args &&... args)
      : functionName_(std::move(functionName)) {
        auto logClient = EasySingleton<TraceManager>::GetInstance();
    logEvent("B", std::forward<Args>(args)...); // "B" for Begin
  }

  ~TraceLogger() {
    logEvent("E"); // "E" for End
  }

 private:
  std::string functionName_;

  static auto getTime(){
    auto now = std::chrono::system_clock::now();
    auto duration_since_epoch = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration_since_epoch).count();
  }

  static auto getTID(){
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
  }

  template<typename T, typename = void>
  struct is_streamable : std::false_type {};

  template<typename T>
  struct is_streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>> : std::true_type {};

  template<typename T>
  std::string stringifyArg(const T& arg) {
    if constexpr (is_streamable<T>::value) {
      std::ostringstream ss;
      ss << arg;
      return ss.str();
    } else {
      return "[non-streamable type]";
    }
  }

  template<typename... Args>
  std::string logArguments(Args&&... args) {
    std::ostringstream ss;
    ss << "{\n";
    int unpack[] = {0, (ss << "  \"" << demangle(typeid(Args).name()) << "\": " << stringifyArg(args) << ",\n", 0)...};
    static_cast<void>(unpack); // To avoid unused variable warning
    std::string result = ss.str();
    if (sizeof...(Args) > 0) {
      // Remove the last comma
      result.erase(result.rfind(','), 1);
    }
    result += "}";
    return result;
  }

  template<typename... Args>
  void logEvent(const std::string &phase, Args &&... args) {
    auto microseconds = getTime();
    auto tid = getTID();

    std::string logEntry = "\"name\": \"" + functionName_
        + "\", \"ph\": \"" + phase
        + "\", \"ts\": " + std::to_string(microseconds)
        + ", \"pid\": " + std::to_string(getpid())
        + ", \"tid\": " + tid;

    if constexpr (sizeof...(args) != 0) {
      std::string argsJson = logArguments(std::forward<Args>(args)...);
      logEntry += ", \"args\":" + argsJson;
    }

    auto log = spdlog::get("trace_logger");
    log->info(logEntry);
  }
};

#endif //DISTRIBUTEDTRACER_TRACER_H
