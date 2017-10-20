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

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sys/time.h>

namespace hycast {

void timeStamp()
{
    struct timeval now;
    ::gettimeofday(&now, nullptr);
    struct tm tm;
    ::gmtime_r(&now.tv_sec, &tm);
    ::fprintf(stderr,
            "%04d%02d%02dT%02d%02d%02d.%06ldZ ",
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min,
            tm.tm_sec, static_cast<long>(now.tv_usec));
}

std::string placeStamp(
        const char* const file,
        const int         line)
{
    char name[::strlen(file)+1];
    ::strcpy(name, file);
    return std::string(::basename(name)) + ":" + std::to_string(line);
}

std::string placeStamp(
        const char* const file,
        const int         line,
        const char* const func)
{
    char name[::strlen(file)+1];
    ::strcpy(name, file);
    return std::string(::basename(name)) + ":" + std::to_string(line) + ":" +
            func + "()";
}

std::string makeWhat(
        const char*        file,
        const int          line,
        const std::string& msg)
{
    return placeStamp(file, line) + " " + msg;
}

std::string makeWhat(
        const char*        file,
        const int          line,
        const char* const  func,
        const std::string& msg)
{
    return placeStamp(file, line, func) + ": " + msg;
}

void log_log(const std::string& msg)
{
    static std::mutex           mutex{};
    std::lock_guard<std::mutex> lock{mutex};
    std::cerr << msg << std::endl;
}

void log_log(
        const char* const  file,
        const int          line,
        const std::string& msg)
{
    log_log(makeWhat(file, line, msg));
}

void log_log(
        const char* const  file,
        const int          line,
        const char* const  func,
        const std::string& msg)
{
    log_log(makeWhat(file, line, func, msg));
}

void log_notice_old(const std::string& msg)
{
    log_log("NOTICE: " + msg);
}

void log_debug(const std::string& msg)
{
    log_log("DEBUG: " + msg);
}

LogLevel                     logLevel = LOG_NOTE;
static const char*           logLevelNames[] = {"DEBUG", "INFO", "NOTE", "WARN",
        "ERROR"};
static std::recursive_mutex  mutex;

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
    std::lock_guard<decltype(mutex)> lock{mutex};
    timeStamp();
    ::fprintf(stderr, "%s %s\n", logLevelNames[level], ex.what());
    ::fflush(stderr);
}

void log(
        const LogLevel level,
        const char*    file,
        const int      line,
        const char*    fmt,
        va_list        argList)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    timeStamp();
    ::fprintf(stderr, "%s %s ", logLevelNames[level],
            placeStamp(file, line).c_str());
    ::vfprintf(stderr, fmt, argList);
    ::fputc('\n', stderr);
    ::fflush(stderr);
}

void log(
        const LogLevel level,
        const char*    file,
        const int      line,
        const char*    fmt,
        ...)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    va_list argList;
    va_start(argList, fmt);
    log(level, file, line, fmt, argList);
    va_end(argList);
}

void log(
        const LogLevel        level,
        const char*           file,
        const int             line,
        const std::exception& ex,
        const char*           fmt,
        ...)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    log(level, ex);
    va_list argList;
    va_start(argList, fmt);
    log(level, file, line, fmt, argList);
    va_end(argList);
}

void log(
        const LogLevel    level,
        const char*       file,
        const int         line,
        const std::string msg)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    timeStamp();
    std::cerr << logLevelNames[level] << " " << placeStamp(file, line).c_str()
            << " " << msg << std::endl;
    std::cerr.flush();
}

void log(
        const LogLevel        level,
        const char*           file,
        const int             line,
        const std::exception& ex,
        const std::string     msg)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    log(level, ex);
    log(level, file, line, msg);
}

} // namespace
