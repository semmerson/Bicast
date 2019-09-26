/**
 * This file declares the exception and logging mechanism for the package.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: error.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_ERROR_H_
#define MAIN_MISC_ERROR_H_

#include <atomic>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>

namespace hycast {

class InvalidArgument : public std::invalid_argument
{
public:
    InvalidArgument(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

#define INVALID_ARGUMENT(msg) InvalidArgument(__FILE__, __LINE__, __func__, msg)
};

class LogicError : public std::logic_error
{
public:
    LogicError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

#define LOGIC_ERROR(msg) LogicError(__FILE__, __LINE__, __func__, msg)
};

class NotFoundError : public std::runtime_error
{
public:
    NotFoundError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

#define NOT_FOUND_ERROR(msg) NotFoundError(__FILE__, __LINE__, __func__, msg)
};

class DomainError : public std::domain_error
{
public:
    DomainError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

#define DOMAIN_ERROR(msg) DomainError(__FILE__, __LINE__, __func__, msg)
};

class OutOfRange : public std::out_of_range
{
public:
    OutOfRange(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);

#define OUT_OF_RANGE(msg) OutOfRange(__FILE__, __LINE__, __func__, msg)
};

class RuntimeError : public std::runtime_error
{
public:
    RuntimeError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg);
};
#define RUNTIME_ERROR(msg) RuntimeError(__FILE__, __LINE__, __func__, msg)

class SystemError : public std::system_error
{
public:
    SystemError(
            const char*       file,
            const int         line,
            const char*       func,
            const std::string msg,
            const int         errnum = errno);

#define SYSTEM_ERROR(msg, ...) SystemError(__FILE__, __LINE__, __func__, msg, \
    ##__VA_ARGS__)
};

std::string makeWhat(
        const char*        file,
        const int          line,
        const char*        func,
        const std::string& msg);

typedef enum {
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_NOTE,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
}                              LogLevel;
typedef std::atomic<LogLevel>  LogThreshold;
extern LogThreshold            logThreshold;

void log_setName(const std::string& name);

/**
 * @cancellationpoint No
 */
void log_setLevel(const LogLevel level) noexcept;

/**
 * @cancellationpoint No
 */
bool log_enabled(const LogLevel level) noexcept;

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
        const LogLevel    level,
        const char*       file,
        const int         line,
        const char*       func,
        const std::string msg);
void log(
        const LogLevel        level,
        const char*           file,
        const int             line,
        const char*           func,
        const std::exception& ex,
        const std::string     msg);

inline void log_debug(const std::exception& ex) {
    if (logThreshold <= LOG_LEVEL_DEBUG)
        log(LOG_LEVEL_DEBUG, ex);
}
inline void log_info(const std::exception& ex) {
    if (logThreshold <= LOG_LEVEL_INFO)
        log(LOG_LEVEL_INFO, ex);
}
inline void log_note(const std::exception& ex) {
    if (logThreshold <= LOG_LEVEL_NOTE)
        log(LOG_LEVEL_NOTE, ex);
}
inline void log_warn(const std::exception& ex) {
    if (logThreshold <= LOG_LEVEL_WARN)
        log(LOG_LEVEL_WARN, ex);
}
inline void log_error(const std::exception& ex) {
    if (logThreshold <= LOG_LEVEL_ERROR)
        log(LOG_LEVEL_ERROR, ex);
}
inline void log_fatal(const std::exception& ex) {
    if (logThreshold <= LOG_LEVEL_FATAL)
        log(LOG_LEVEL_FATAL, ex);
}

#define LOG_TRACE(...) \
    do \
        if (hycast::logThreshold <= hycast::LOG_LEVEL_TRACE) \
            hycast::log(hycast::LOG_LEVEL_TRACE, __FILE__, __LINE__, \
                    __func__); \
    while(false)

#define LOG_DEBUG(...) \
    do \
        if (hycast::logThreshold <= hycast::LOG_LEVEL_DEBUG) \
            hycast::log(hycast::LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_INFO(...) \
    do \
        if (hycast::logThreshold <= hycast::LOG_LEVEL_INFO) \
            hycast::log(hycast::LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_NOTE(...) \
    do \
        if (hycast::logThreshold <= hycast::LOG_LEVEL_NOTE) \
            hycast::log(hycast::LOG_LEVEL_NOTE, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_WARN(...) \
    do \
        if (hycast::logThreshold <= hycast::LOG_LEVEL_WARN) \
            hycast::log(hycast::LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_ERROR(...) \
    do \
        if (hycast::logThreshold <= hycast::LOG_LEVEL_ERROR) \
            hycast::log(hycast::LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

#define LOG_FATAL(...) \
    do \
        if (hycast::logThreshold <= hycast::LOG_LEVEL_FATAL) \
            hycast::log(hycast::LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, \
                    __VA_ARGS__); \
    while(false)

} // namespace

#endif /* MAIN_MISC_ERROR_H_ */
