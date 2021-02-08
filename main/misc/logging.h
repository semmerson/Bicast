/**
 * This file declares the API for logging.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: logging.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_LOGGING_H_
#define MAIN_LOGGING_H_

#include <atomic>
#include <exception>
#include <iostream>
#include <signal.h>

namespace hycast {

class LogLevel
{
    int level;

    LogLevel(const int level) noexcept
        : level{level}
    {}

public:
    static const LogLevel TRACE;
    static const LogLevel DEBUG;
    static const LogLevel INFO;
    static const LogLevel NOTE;
    static const LogLevel WARN;
    static const LogLevel ERROR;
    static const LogLevel FATAL;

    LogLevel() noexcept
        : LogLevel(0)
    {}

    operator int() const noexcept {
        return level;
    }

    bool includes(const LogLevel& arg) const noexcept {
        return arg.level >= level;
    }

    void lower() noexcept {
        if (level)
            --level;
    }

    const std::string& to_string() const noexcept {
        static const std::string strings[] = {
                "TRACE", "DEBUG", "INFO", "NOTE", "WARN", "ERROR", "FATAL"
        };
        return strings[level];
    }
};

typedef std::atomic<LogLevel> LogThreshold;
extern LogThreshold           logThreshold;

void log_setName(const std::string& name);

const std::string& log_getName() noexcept;

void log_setLevelSignal(const int signal = SIGUSR2) noexcept;

/**
 * Returns the current logging level.
 *
 * @return  Current logging level
 */
LogLevel log_getLevel() noexcept;

/**
 * @cancellationpoint No
 */
void log_setLevel(const LogLevel level) noexcept;

/**
 * Sets the logging level. Useful in command-line decoding.
 *
 * @param[in] name               Name of the logging level. One of "trace",
 *                               "debug", "info", "note", "warn", "error", or
 *                               "fatal". Fewer characters can be used. Matching
 *                               is case independent.
 * @throw std::invalid_argument  Name isn't one of the allowed names
 */
void log_setLevel(const std::string& name);

/**
 * @cancellationpoint No
 */
inline bool log_enabled(const LogLevel& level) noexcept {
    return logThreshold.load().includes(level);
}

void log(
        const LogLevel        level,
        const std::exception& ex);
void log(
        const LogLevel level,
        const char*    file,
        const int      line,
        const char*    func,
        const char*    fmt,
        va_list        argList);
void log(
        const LogLevel level,
        const char*    file,
        const int      line,
        const char*    func);
void log(
        const LogLevel level,
        const char*    file,
        const int      line,
        const char*    func,
        const char*    fmt,
        ...);
void log(
        const LogLevel        level,
        const char*           file,
        const int             line,
        const char*           func,
        const std::exception& ex,
        const char*           fmt,
        ...);
void log(
        const LogLevel     level,
        const char*        file,
        const int          line,
        const char*        func,
        const std::string& msg);
void log(
        const LogLevel        level,
        const char*           file,
        const int             line,
        const char*           func,
        const std::exception& ex);

inline void log_debug(const std::exception& ex) {
    if (log_enabled(LogLevel::DEBUG))
        log(LogLevel::DEBUG, ex);
}
inline void log_info(const std::exception& ex) {
    if (log_enabled(LogLevel::INFO))
        log(LogLevel::INFO, ex);
}
inline void log_note(const std::exception& ex) {
    if (log_enabled(LogLevel::NOTE))
        log(LogLevel::NOTE, ex);
}
inline void log_warn(const std::exception& ex) {
    if (log_enabled(LogLevel::WARN))
        log(LogLevel::WARN, ex);
}
inline void log_error(const std::exception& ex) {
    if (log_enabled(LogLevel::ERROR))
        log(LogLevel::ERROR, ex);
}
inline void log_fatal(const std::exception& ex) {
    if (log_enabled(LogLevel::FATAL))
        log(LogLevel::FATAL, ex);
}

#define LOG_TRACE(...) \
    do \
        if (log_enabled(LogLevel::TRACE)) \
            hycast::log(hycast::LogLevel::TRACE, __FILE__, __LINE__, \
                    __func__); \
    while(false)

#define LOG_DEBUG(...) \
    do \
        if (hycast::log_enabled(hycast::LogLevel::DEBUG)) \
            hycast::log(hycast::LogLevel::DEBUG, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_INFO(...) \
    do \
        if (hycast::log_enabled(hycast::LogLevel::INFO)) \
            hycast::log(hycast::LogLevel::INFO, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_NOTE(...) \
    do \
        if (hycast::log_enabled(hycast::LogLevel::NOTE)) \
            hycast::log(hycast::LogLevel::NOTE, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_WARN(...) \
    do \
        if (hycast::log_enabled(hycast::LogLevel::WARN)) \
            hycast::log(hycast::LogLevel::WARN, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_ERROR(...) \
    do \
        if (hycast::log_enabled(hycast::LogLevel::ERROR)) \
            hycast::log(hycast::LogLevel::ERROR, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_FATAL(...) \
    do \
        if (hycast::log_enabled(hycast::LogLevel::FATAL)) \
            hycast::log(hycast::LogLevel::FATAL, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

} // namespace

#endif /* MAIN_LOGGING_H_ */
