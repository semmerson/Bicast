/**
 * This file declares the API for logging.
 *
 *   @file: logging.h
 * @author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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

#ifndef MAIN_LOGGING_H_
#define MAIN_LOGGING_H_

#include <atomic>
#include <signal.h> // For SIGUSR2
#include <string>

namespace bicast {

/// Logging threshold
class LogLevel
{
public:
    /*
    static const LogLevel TRACE; ///< Trace messages
    static const LogLevel DEBUG; ///< Debug messages
    static const LogLevel INFO;  ///< Informational messages
    static const LogLevel NOTE;  ///< Notices
    static const LogLevel WARN;  ///< Warnings
    static const LogLevel ERROR; ///< Errors
    static const LogLevel FATAL; ///< Fatal errors
    */

    /// Logging priorities
    enum Priority {
        TRACE, ///< Trace messages
        DEBUG, ///< Debug messages
        INFO,  ///< Informational messages
        NOTE,  ///< Notices
        WARN,  ///< Warnings
        ERROR, ///< Errors
        FATAL  ///< Fatal errors
    };

    /**
     * Default constructs.
     */
    LogLevel()
        : priority(NOTE)
    {}

    /**
     * Constructs from a priority.
     * @param[in] priority  The logging priority
     */
    LogLevel(const int priority)
        : priority(priority)
    {}

    /**
     * Copy constructs.
     * @param[in] level  Another instance
     */
    LogLevel(const LogLevel& level)
        : LogLevel(level.priority.load())
    {}

    /**
     * Copy assigns.
     * @param[in] rhs  Priority
     * @return         This instance
     */
    LogLevel& operator=(const LogLevel& rhs) {
        priority.store(rhs.priority.load());
        return *this;
    }

    /**
     * Assigns from a priority.
     * @param[in] rhs  Priority
     * @return         This instance
     */
    LogLevel& operator=(const Priority rhs) {
        priority.store(rhs);
        return *this;
    }

    /**
     * Returns this instance cast to an int.
     * @return This instance as an int
     */
    operator int() const noexcept {
        return priority;
    }

    operator bool() const = delete;

    /**
     * Indicates if this instance is considered equal to another.
     * @param[in] rhs     The other instance
     * @retval true       This instance does equal the other
     * @retval false      This instance does not equal the other
     */
    bool operator==(LogLevel rhs) const noexcept {
        return priority == rhs.priority;
    }

    /**
     * Indicates if this instance is not considered equal to another.
     * @param[in] rhs     The other instance
     * @retval true       This instance does not equal the other
     * @retval false      This instance does equal the other
     */
    bool operator!=(LogLevel rhs) const noexcept {
        return priority != rhs.priority;
    }

    /**
     * Indicates if the current logging level includes a given one.
     * @param[in] level  The given logging level to be examined
     * @retval    true   The current logging level includes the given one
     * @retval    false  The current logging level does not include the given one
     */
    bool includes(const LogLevel level) const noexcept {
        return priority <= level.priority;
    }

    /**
     * Lowers the priority threshold making logging more verbose.
     */
    void lower() noexcept {
        int expect = TRACE;
        if (!priority.compare_exchange_strong(expect, expect))
            --priority;
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    const std::string& to_string() const noexcept {
        static const std::string strings[] = {
                "TRACE", "DEBUG", "INFO", "NOTE", "WARN", "ERROR", "FATAL"
        };
        return strings[priority];
    }

private:
    std::atomic_int priority;
};

std::ostream& operator<<(std::ostream& ostream, const LogLevel& level);

/**
 * Returns a single-string representation of the arguments of a command line.
 * @param[in] argc  Number of arguments
 * @param[in] argv  Arguments
 * @return          Single string representation
 */
std::string getCmdLine(
        int                argc,
        const char* const* argv);

extern LogLevel logThreshold; ///< The logging threshold

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
void log_setLevel(const LogLevel::Priority priority) noexcept;

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
    return logThreshold.includes(level);
}

void log(
        const LogLevel        level,
        const std::exception& ex);
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

/**
 * Logs an exception at the trace level.
 * @param[in] ex  The exception
 */
inline void log_trace(const std::exception& ex) {
    if (log_enabled(LogLevel::TRACE))
        log(LogLevel::TRACE, ex);
}
/**
 * Logs an exception at the debug level.
 * @param[in] ex  The exception
 */
inline void log_debug(const std::exception& ex) {
    if (log_enabled(LogLevel::DEBUG))
        log(LogLevel::DEBUG, ex);
}
/**
 * Logs an exception at the info level.
 * @param[in] ex  The exception
 */
inline void log_info(const std::exception& ex) {
    if (log_enabled(LogLevel::INFO))
        log(LogLevel::INFO, ex);
}
/**
 * Logs an exception at the NOTE level.
 * @param[in] ex  The exception
 */
inline void log_note(const std::exception& ex) {
    if (log_enabled(LogLevel::NOTE))
        log(LogLevel::NOTE, ex);
}
/**
 * Logs an exception at the WARN level.
 * @param[in] ex  The exception
 */
inline void log_warn(const std::exception& ex) {
    if (log_enabled(LogLevel::WARN))
        log(LogLevel::WARN, ex);
}
/**
 * Logs an exception at the ERROR level.
 * @param[in] ex  The exception
 */
inline void log_error(const std::exception& ex) {
    if (log_enabled(LogLevel::ERROR))
        log(LogLevel::ERROR, ex);
}
/**
 * Logs an exception at the FATAL level.
 * @param[in] ex  The exception
 */
inline void log_fatal(const std::exception& ex) {
    if (log_enabled(LogLevel::FATAL))
        log(LogLevel::FATAL, ex);
}

/// Macro for logging a message at the trace level
#define LOG_TRACE(...) \
    do \
        if (bicast::log_enabled(bicast::LogLevel::TRACE)) \
            bicast::log(bicast::LogLevel::TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    while(false)

/// Macro for logging a message at the debug level
#define LOG_DEBUG(...) \
    do \
        if (bicast::log_enabled(bicast::LogLevel::DEBUG)) \
            bicast::log(bicast::LogLevel::DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    while(false)

/// Macro for logging a message at the info level
#define LOG_INFO(...) \
    do \
        if (bicast::log_enabled(bicast::LogLevel::INFO)) \
            bicast::log(bicast::LogLevel::INFO, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    while(false)

/// Macro for logging a message at the note level
#define LOG_NOTE(...) \
    do \
        if (bicast::log_enabled(bicast::LogLevel::NOTE)) \
            bicast::log(bicast::LogLevel::NOTE, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    while(false)

/// Macro for logging a message at the warning level
#define LOG_WARN(...) \
    do \
        if (bicast::log_enabled(bicast::LogLevel::WARN)) \
            bicast::log(bicast::LogLevel::WARN, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    while(false)

/// Macro for logging a message at the error level
#define LOG_ERROR(...) \
    do \
        if (bicast::log_enabled(bicast::LogLevel::ERROR)) \
            bicast::log(bicast::LogLevel::ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    while(false)

/// Macro for logging a message at the system-error level
#define LOG_SYSERR(...) \
    do \
        if (bicast::log_enabled(bicast::LogLevel::ERROR)) { \
            bicast::log(bicast::LogLevel::ERROR, __FILE__, __LINE__, __func__, "%s", \
                    ::strerror(errno)); \
            bicast::log(bicast::LogLevel::ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } \
    while(false)

/// Macro for logging a message at the fatal level
#define LOG_FATAL(...) \
    do \
        if (bicast::log_enabled(bicast::LogLevel::FATAL)) \
            bicast::log(bicast::LogLevel::FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    while(false)


/**
 * Logs an error message and then aborts the current process.
 *
 * @param[in] ...  Optional arguments of the message -- starting with the
 *                 format of the message.
 */
#define LOG_ABORT(...) do { \
    LOG_FATAL(__VA_ARGS__); \
    ::abort(); \
} while (false)

#ifdef NDEBUG
    #define LOG_ASSERT(expr)
#else
    /**
     * Tests an assertion. Logs an error-message and then aborts the process
     * if the assertion is false.
     *
     * @param[in] expr  The assertion to be tested.
     */
    #define LOG_ASSERT(expr) do { \
        if (!(expr)) \
            LOG_ABORT("Assertion failure: %s", #expr); \
    } while (false)
#endif

} // namespace

#endif /* MAIN_LOGGING_H_ */
