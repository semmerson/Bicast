/**
 * This file implements logging.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: logging.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"
#include "Thread.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>
#include <sys/time.h>
#include <unistd.h>

namespace hycast {

static int LOC_WIDTH = 32;

class StreamGuard
{
    FILE* stream;

public:
    StreamGuard(FILE* stream) : stream{stream} {
        ::flockfile(stream);
    }

    StreamGuard(const StreamGuard& guard) =delete;

    StreamGuard& operator=(const StreamGuard& rhs) =delete;

    ~StreamGuard() {
        ::funlockfile(stream);
    }
};

static std::string progName{"<unset>"};

static int timeStamp(FILE* stream)
{
    struct timeval now;
    ::gettimeofday(&now, nullptr);
    struct tm tm;
    ::gmtime_r(&now.tv_sec, &tm);
    return ::fprintf(stream,
            "%04d%02d%02dT%02d%02d%02d.%06ldZ",
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min,
            tm.tm_sec, static_cast<long>(now.tv_usec));
}

static void procStamp(FILE* stream)
{
    char               progField[80];
    std::ostringstream threadId;

    threadId << std::this_thread::get_id();
    ::snprintf(progField, sizeof(progField), "%s:%d:%s", progName.c_str(),
            getpid(), threadId.str().c_str());

    ::fprintf(stream, "%-35s", progField);
}

static std::string codeStamp(
        const char* const file,
        const int         line,
        const char* const func)
{
    char name[::strlen(file)+1];
    ::strcpy(name, file);
    return std::string(::basename(name)) + ":" + func + ":" +
            std::to_string(line);
}

static const char*         logLevelNames[] = {"TRACE", "DEBUG", "INFO",
        "NOTE", "WARN", "ERROR", "FATAL"};

static void logHeader(
        const LogLevel    level,
        const char* const file,
        const int         line,
        const char* const func)
{
    // Time stamp
    timeStamp(stderr);

    // Program, process, and thread
    ::fputc(' ', stderr);
    procStamp(stderr);

    // Logging level
    ::fputc(' ', stderr);
    ::fprintf(stderr, "%-5s", logLevelNames[level]);

    // Code location
    ::fputc(' ', stderr);
    ::fprintf(stderr, "%-*s", LOC_WIDTH, codeStamp(file, line, func).c_str());
}

LogThreshold logThreshold(LOG_LEVEL_NOTE);

void log_setName(const std::string& name) {
    progName = name;
}

void log_setLevel(const LogLevel level) noexcept {
    logThreshold.store(level);
}

bool log_enabled(const LogLevel level) noexcept {
    return level >= logThreshold;
}

std::string makeWhat(
        const char*        file,
        const int          line,
        const char* const  func,
        const std::string& msg)
{
    auto what = codeStamp(file, line, func);

    for (int n = what.size(); n < LOC_WIDTH; ++n)
        what += ' ';

    what += ' ' + msg;

    return what;
}

void log(
        const LogLevel    level,
        const char* const file,
        const int         line,
        const char* const func,
        const char* const fmt,
        va_list           argList)
{
    StreamGuard guard(stderr);

    logHeader(level, file, line, func);

    // Message
    ::fputc(' ', stderr);
    ::vfprintf(stderr, fmt, argList);

    ::fputc('\n', stderr);
    ::fflush(stderr);
}

void log(
        const LogLevel        level,
        const std::exception& ex)
{
    try {
        std::rethrow_if_nested(ex);
    }
    catch (const std::exception& inner) {
        log(level, inner);
    }

    StreamGuard guard(stderr);

    timeStamp(stderr);

    // Program, process, and thread
    ::fputc(' ', stderr);
    procStamp(stderr);

    // Logging level
    ::fputc(' ', stderr);
    ::fprintf(stderr, "%-5s", logLevelNames[level]);

    // Code location and message
    ::fputc(' ', stderr);
    ::fprintf(stderr, "%s", ex.what());

    ::fputc('\n', stderr);
    ::fflush(stderr);
}

void log(
        const LogLevel    level,
        const char* const file,
        const int         line,
        const char* const func)
{
    StreamGuard guard(stderr);

    logHeader(level, file, line, func);

    ::fputc('\n', stderr);
    ::fflush(stderr);
}

void log(
        const LogLevel    level,
        const char* const file,
        const int         line,
        const char* const func,
        const char* const fmt,
        ...)
{
    StreamGuard guard(stderr);
    va_list     argList;

    va_start(argList, fmt);
    log(level, file, line, func, fmt, argList);
    va_end(argList);
}

void log(
        const LogLevel        level,
        const char*           file,
        const int             line,
        const char* const     func,
        const std::exception& ex,
        const char*           fmt,
        ...)
{
    StreamGuard guard(stderr);
    va_list     argList;

    log(level, ex);

    va_start(argList, fmt);
    log(level, file, line, func, fmt, argList);
    va_end(argList);
}

} // namespace
