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

#ifdef MPI_VERSION
#include <mpi.h>

#define INIT_TRACE_MANAGER() \
    static int _traceRank;   \
    MPI_Comm_rank(MPI_COMM_WORLD, &_traceRank); \
    TraceManager traceManager(_traceRank);

#define TRACE_FUNC() \
    static int _traceRank;   \
    MPI_Comm_rank(MPI_COMM_WORLD, &_traceRank); \
    TraceLogger traceLogger(__func__, _traceRank)

#else
#define INIT_TRACE_MANAGER(_traceRank) \
    TraceManager traceManager(_traceRank);

#define TRACE_FUNC(_traceRank) \
    TraceLogger traceLogger(__func__, _traceRank)
#endif


class TraceManager {
public:
    explicit TraceManager(int rank){
        std::string fileName = "trace_" + std::to_string(rank) + ".json";
        spdlog::basic_logger_mt("trace_logger", fileName);

        auto log = spdlog::get("trace_logger");
        log->set_pattern(entryPattern);
        log->info("");
        log->set_pattern(jsonPattern);
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
};

class TraceLogger {
public:
    TraceLogger(std::string functionName, int rank)
            : functionName_(std::move(functionName)), rank_(rank) {
        logEvent("B"); // "B" for Begin
    }

    ~TraceLogger() {
        logEvent("E"); // "E" for End
    }

private:
    std::string functionName_;
    int rank_;

    void logEvent(const std::string &phase) {
        auto now = std::chrono::system_clock::now();
        auto duration_since_epoch = now.time_since_epoch();
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration_since_epoch).count();
        std::stringstream ss;
        ss << std::this_thread::get_id();
        std::string threadID = ss.str();

        std::string logEntry = "\"name\": \"" + functionName_
                               + "\", \"ph\": \"" + phase
                               + "\", \"ts\": " + std::to_string(microseconds)
                               + ", \"pid\": " + std::to_string(rank_)
                               + ", \"tid\": " + threadID
                               + "";

        auto log = spdlog::get("trace_logger");
        log->info(logEntry);
    }
};


#endif //DISTRIBUTEDTRACER_TRACER_H

