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
            const std::string msg);

#define INVALID_ARGUMENT(msg) InvalidArgument(__FILE__, __LINE__, msg)
};

class LogicError : public std::logic_error
{
public:
    LogicError(
            const char*       file,
            const int         line,
            const std::string msg);

#define LOGIC_ERROR(msg) LogicError(__FILE__, __LINE__, msg)
};

class NotFoundError : public std::runtime_error
{
public:
    NotFoundError(
            const char*       file,
            const int         line,
            const std::string msg);

#define NOT_FOUND_ERROR(msg) NotFoundError(__FILE__, __LINE__, msg)
};

class OutOfRange : public std::out_of_range
{
public:
    OutOfRange(
            const char*       file,
            const int         line,
            const std::string msg);

#define OUT_OF_RANGE(msg) OutOfRange(__FILE__, __LINE__, msg)
};

class RuntimeError : public std::runtime_error
{
public:
    RuntimeError(
            const char*       file,
            const int         line,
            const std::string msg);
};
#define RUNTIME_ERROR(msg) RuntimeError(__FILE__, __LINE__, msg)

class SystemError : public std::system_error
{
public:
    SystemError(
            const char*       file,
            const int         line,
            const std::string msg,
            const int         errnum = errno);

#define SYSTEM_ERROR(msg, ...) SystemError(__FILE__, __LINE__, msg, ##__VA_ARGS__)
};

void timeStamp();
std::string placeStamp(
        const char* const file,
        const int         line);
std::string placeStamp(
        const char* const file,
        const int         line,
        const char* const func);
std::string makeWhat(
        const char* const  file,
        const int          line,
        const char* const  func,
        const std::string& msg);
std::string makeWhat(
        const char*        file,
        const int          line,
        const std::string& msg);

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_NOTE,
    LOG_WARN,
    LOG_ERROR
} LogLevel;
extern LogLevel logLevel;

inline bool log_enabled(const LogLevel level) {
    return level <= logLevel;
}

void log(
        const LogLevel        level,
        const std::exception& ex);
void log(
        const LogLevel level,
        const char*    file,
        const int      line,
        const char*    fmt,
        va_list        argList);
void log(
        const LogLevel level,
        const char*    file,
        const int      line,
        const char*    fmt,
        ...);
void log(
        const LogLevel        level,
        const char*           file,
        const int             line,
        const std::exception& ex,
        const char*           fmt,
        ...);
void log(
        const LogLevel    level,
        const char*       file,
        const int         line,
        const std::string msg);
void log(
        const LogLevel        level,
        const char*           file,
        const int             line,
        const std::exception& ex,
        const std::string     msg);

inline void log_debug(const std::exception& ex) {
    if (logLevel <= LOG_DEBUG)
        log(LOG_DEBUG, ex);
}
inline void log_info(const std::exception& ex) {
    if (logLevel <= LOG_INFO)
        log(LOG_INFO, ex);
}
inline void log_note(const std::exception& ex) {
    if (logLevel <= LOG_NOTE)
        log(LOG_NOTE, ex);
}
inline void log_warn(const std::exception& ex) {
    if (logLevel <= LOG_WARN)
        log(LOG_WARN, ex);
}
inline void log_error(const std::exception& ex) {
    if (logLevel <= LOG_ERROR)
        log(LOG_ERROR, ex);
}

#define LOG_DEBUG(...) \
    do \
        if (hycast::logLevel <= hycast::LOG_DEBUG) \
            hycast::log(hycast::LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__); \
    while(false)

#define LOG_INFO(...) \
    do \
        if (hycast::logLevel <= hycast::LOG_INFO) \
            hycast::log(hycast::LOG_INFO, __FILE__, __LINE__, __VA_ARGS__); \
    while(false)

#define LOG_NOTE(...) \
    do \
        if (hycast::logLevel <= hycast::LOG_NOTE) \
            hycast::log(hycast::LOG_NOTE, __FILE__, __LINE__, __VA_ARGS__); \
    while(false)

#define LOG_WARN(...) \
    do \
        if (hycast::logLevel <= hycast::LOG_WARN) \
            hycast::log(hycast::LOG_WARN, __FILE__, __LINE__, __VA_ARGS__); \
    while(false)

#define LOG_ERROR(...) \
    do \
        if (hycast::logLevel <= hycast::LOG_ERROR) \
            hycast::log(hycast::LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__); \
    while(false)

} // namespace

#endif /* MAIN_MISC_ERROR_H_ */
