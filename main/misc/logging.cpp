/**
 * This file implements logging.
 *
 *   @file: logging.cpp
 * @author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "error.h"
#include "Thread.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <sys/time.h>
#include <unistd.h>

namespace hycast {

static int LOC_WIDTH = 32;

/// Synchronizes access to an I/O stream
class StreamGuard
{
    FILE*      stream;
    std::mutex mutex;

public:
    /**
     * Constructs.
     * @param[in] stream  Underlying stream to be guarded
     */
    StreamGuard(FILE* stream)
        : stream{stream}
        , mutex()
    {
        struct flock flock = {0};
        flock.l_type = F_WRLCK;
        flock.l_whence = SEEK_SET;

        mutex.lock();
        ::fcntl(::fileno(stream), F_SETLKW, &flock);  // Can't hurt?
        ::flockfile(stream);
    }

    /**
     * Copy constructs.
     * @param[in] guard  The other instance
     */
    StreamGuard(const StreamGuard& guard) =delete;

    /**
     * Copy assigns.
     * @param[in] rhs  The other instance
     * @return         Reference to the assigned instance
     */
    StreamGuard& operator=(const StreamGuard& rhs) =delete;

    ~StreamGuard() {
        struct flock flock = {0};
        flock.l_type = F_UNLCK;
        flock.l_whence = SEEK_SET;

        ::funlockfile(stream);
        ::fcntl(::fileno(stream), F_SETLK, &flock);
        mutex.unlock();
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
    return std::string(::basename(name)) + ":" + std::to_string(line) + ":" +
            func;
}

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
    ::fprintf(stderr, "%-5s", level.to_string().data());

    // Code location
    ::fputc(' ', stderr);
    ::fprintf(stderr, "%-*s", LOC_WIDTH, codeStamp(file, line, func).c_str());
}

const LogLevel LogLevel::TRACE{0};
const LogLevel LogLevel::DEBUG{1};
const LogLevel LogLevel::INFO{2};
const LogLevel LogLevel::NOTE{3};
const LogLevel LogLevel::WARN{4};
const LogLevel LogLevel::ERROR{5};
const LogLevel LogLevel::FATAL{6};

LogThreshold logThreshold(LogLevel::NOTE); ///< The current logging threshold

/**
 * Sets the name of the program.
 * @param[in] name The name of the program
 */
void log_setName(const std::string& name) {
    progName = name;
}

/**
 * Returns the name of the program.
 * @return The name of the program
 */
const std::string& log_getName() noexcept {
    return progName;
}

/**
 * Signal handler for rotating the logging threshold
 * @param[in] sig  The signal. Ignored.
 */
static void rollLevel(const int sig)
{
    LogLevel level = static_cast<LogLevel>(logThreshold);

    if (level.includes(LogLevel::TRACE)) {
        level = LogLevel::NOTE;
    }
    else {
        level.lower();
    }
    logThreshold = level;
}

/**
 * Sets the signal for rotating the logging threshold.
 * @param[in] signal  The signal
 */
void log_setLevelSignal(const int signal) noexcept {
    struct sigaction sigact;
    (void) sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = &rollLevel;
    sigact.sa_flags |= SA_RESTART;
    (void)sigaction(SIGUSR2, &sigact, NULL);

    sigset_t sigset;
    (void)sigemptyset(&sigset);
    (void)sigaddset(&sigset, SIGUSR2);
    (void)sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}

void log_setLevel(const std::string& name)
{
    // To prevent `entry->id.find(lowerName)` from returning 0
    if (name.empty())
        throw INVALID_ARGUMENT("Empty string");

    static const struct Entry {
        std::string     id;
        const LogLevel& level;
    } entries[] = {
        {"TRACE", LogLevel::TRACE},
        {"DEBUG", LogLevel::DEBUG},
        {"INFO",  LogLevel::INFO },
        {"NOTE",  LogLevel::NOTE },
        {"WARN",  LogLevel::WARN },
        {"ERROR", LogLevel::ERROR},
        {"FATAL", LogLevel::FATAL},
        {"",      LogLevel()}
    };

    std::string lowerName = name;
    for (auto& c : lowerName)
        c = ::toupper(c);

    const struct Entry* entry;
    for (entry = entries; !entry->id.empty(); ++entry) {
        if (entry->id.find(lowerName) == 0) {
            log_setLevel(entry->level);
            return;
        }
    }

    throw INVALID_ARGUMENT("Invalid logging-level name: \"" + name + "\"");
}

/**
 * Returns the logging level.
 * @return The logging level
 */
LogLevel log_getLevel() noexcept {
    return static_cast<LogLevel>(logThreshold);
}

/**
 * Sets the logging level.
 * @param[in] level  The logging level
 */
void log_setLevel(const LogLevel level) noexcept {
    logThreshold.store(level);
}

/**
 * Returns a constructed message for an exception.
 * @param[in] file  The name of the file in which the exception occurred
 * @param[in] line  The line number in the file in which the exception occurred
 * @param[in] func  The name of the function in the file in which the exception occurred
 * @param[in] msg   The log message
 * @return          The message for the exception
 */
std::string makeWhat(
        const char*        file,
        const int          line,
        const char* const  func,
        const std::string& msg)
{
    //LOG_DEBUG("Entered");
    auto what = codeStamp(file, line, func);

    for (int n = what.size(); n < LOC_WIDTH; ++n)
        what += ' ';

    what += ' ' + msg;

    //LOG_DEBUG("Made what: %s", what.data());
    return what;
}

/**
 * Logs a message.
 * @param[in] level    The logging level to use
 * @param[in] file     The name of the file in which the exception occurred
 * @param[in] line     The line number in the file in which the exception occurred
 * @param[in] func     The name of the function in the file in which the exception occurred
 * @param[in] fmt      The format of the log message
 * @param[in] argList  The argument list for `fmt`
 */
static void log(
        const LogLevel    level,
        const char* const file,
        const int         line,
        const char* const func,
        const char* const fmt,
        va_list           argList)
{
    logHeader(level, file, line, func);

    // Message
    ::fputc(' ', stderr);
    ::vfprintf(stderr, fmt, argList);

    ::fputc('\n', stderr);
    ::fflush(stderr);
}

/**
 * Logs an exception.
 * @param[in] level  The logging level to use
 * @param[in] ex     The exception to log
 */
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
    ::fprintf(stderr, "%-5s", level.to_string().data());

    // Code location and message
    ::fputc(' ', stderr);
    ::fprintf(stderr, "%s", ex.what());

    ::fputc('\n', stderr);
    ::fflush(stderr);
}

/**
 * Logs a message
 * @param[in] level  The logging level to use
 * @param[in] file   The name of the file in which the exception occurred
 * @param[in] line   The line number in the file in which the exception occurred
 * @param[in] func   The name of the function in the file in which the exception occurred
 */
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

/**
 * Logs a message.
 * @param[in] level  The logging level to use
 * @param[in] file   The name of the file in which the exception occurred
 * @param[in] line   The line number in the file in which the exception occurred
 * @param[in] func   The name of the function in the file in which the exception occurred
 * @param[in] fmt    The format of the log message
 * @param[in] ...    The arguments for `fmt`
 */
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

/**
 * Logs an exception.
 * @param[in] level  The logging level to use
 * @param[in] file   The name of the file in which the exception occurred
 * @param[in] line   The line number in the file in which the exception occurred
 * @param[in] func   The name of the function in the file in which the exception occurred
 * @param[in] ex     The exception to log
 */
void log(
        const LogLevel        level,
        const char*           file,
        const int             line,
        const char* const     func,
        const std::exception& ex)
{
    StreamGuard guard(stderr);
    log(level, ex);
}

/**
 * Logs an exception and a message.
 * @param[in] level  The logging level to use
 * @param[in] file   The name of the file in which the exception occurred
 * @param[in] line   The line number in the file in which the exception occurred
 * @param[in] func   The name of the function in the file in which the exception occurred
 * @param[in] ex     The exception to log
 * @param[in] fmt    The format of the log message
 * @param[in] ...    The arguments for `fmt`
 */
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

/**
 * Logs a message.
 * @param[in] level  The logging level to use
 * @param[in] file   The name of the file in which the exception occurred
 * @param[in] line   The line number in the file in which the exception occurred
 * @param[in] func   The name of the function in the file in which the exception occurred
 * @param[in] msg    The log message
 */
void log(
        const LogLevel     level,
        const char*        file,
        const int          line,
        const char*        func,
        const std::string& msg)
{
    log(level, file, line, func, "%s", msg.data());
}

/**
 * Writes a logging level to an output stream.
 * @param[in] ostream  The output stream
 * @param[in] level    The logging level
 * @return             A reference to the output stream
 */
std::ostream& operator<<(std::ostream& ostream, const LogLevel& level) {
    return ostream << level.to_string();
}

std::string getCmdLine(
        int                argc,
        const char* const* argv)
{
    std::string cmdLine = "[";
    bool        needComma = false;

    while (argc-- > 0) {
        if (needComma)
            cmdLine.append(", ");
        cmdLine.append(*argv++);
        needComma = true;
    }

    cmdLine.append("]");

    return cmdLine;
}

} // namespace
