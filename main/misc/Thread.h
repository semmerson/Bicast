/**
 * This file declares a thread-of-control class that supports copying,
 * assignment, and POSIX thread cancellation.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Thread.h
 *  Created on: May 11, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_THREAD_H_
#define MAIN_MISC_THREAD_H_

#include "error.h"

#include <atomic>
#include <functional>
#include <memory>
#include <pthread.h>
#include <thread>

#define THREAD_CLEANUP_PUSH(func, arg) pthread_cleanup_push(func, arg)
#define THREAD_CLEANUP_POP(execute)    pthread_cleanup_pop(execute)

namespace hycast {

class Thread final : public std::thread
{
    std::atomic_bool canceled;
    std::atomic_bool defaultConstructed;

    static void threadCanceled(void* arg);

public:
    friend class std::hash<Thread>;
    friend class std::less<Thread>;

    typedef std::thread::native_handle_type ThreadId;

    using std::thread::join;

    Thread();
    Thread(Thread&) =delete;
    Thread(const Thread&) =delete;
    Thread(Thread&& that);

    template<class Func, class... Args>
    explicit Thread(Func&& func, Args&&... args)
        // Ensure cancelability
        : std::thread([this](decltype(
                  std::bind(func, std::forward<Args>(args)...))&& boundFunc)
                  mutable {
              try {
                  int previous;
                  if (::pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,
                          &previous))
                      throw SystemError(__FILE__, __LINE__,
                              "pthread_setcanceltype() failure");
                  if (::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,
                          &previous))
                      throw SystemError(__FILE__, __LINE__,
                              "pthread_setcancelstate() failure");
              }
              catch (const std::exception& e) {
                  log_what(e, __FILE__, __LINE__, "Couldn't start thread");
              }
              THREAD_CLEANUP_PUSH(threadCanceled, this);
              boundFunc(); // Will call terminate() if exception thrown
              THREAD_CLEANUP_POP(false);
          }, std::bind(func, std::forward<Args>(args)...))
        , canceled{false}
        , defaultConstructed{false}
    {}

    /**
     * Destroys. Because `Thread` is a RAII class, if the associated
     * thread-of-control is joinable, then it is canceled and joined.
     */
    ~Thread();

    void swap(Thread& that) noexcept;

    Thread& operator=(const Thread& rhs) =delete;

    Thread& operator=(Thread&& rhs) noexcept;

    /**
     * Returns the identifier of the current thread.
     * @return           Identifier of the current thread
     * @exceptionsafety  Nothrow
     * @threadsafety     Safe
     */
    static ThreadId getId() noexcept
    {
        return ::pthread_self();
    }

    /**
     * Sets the cancelability of the current thread.
     * @param[in] enable   Whether or not to enable cancelability
     * @return             Previous state
     * @throw SystemError  Cancelability couldn't be set
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    static bool enableCancel(const bool enable = true);

    /**
     * Disables cancelability of the current thread.
     * @return             Previous state
     * @throw SystemError  Cancelability couldn't be set
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    inline static bool disableCancel()
    {
        return enableCancel(false);
    }

    /**
     * Cancels a thread.
     * @throw SystemError  Thread couldn't be canceled
     * @threadsafety       Safe
     */
    static void cancel(const ThreadId& threadId);

    /**
     * Cancels the thread. Idempotent. Does nothing if the thread has been
     * default constructed.
     * @throw SystemError  Thread couldn't be canceled
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    void cancel();
};

void swap(Thread& t1, Thread& t2);

} // namespace

namespace std {
    template<>
    void swap<hycast::Thread>(
            hycast::Thread& a,
            hycast::Thread& b);

    template<>
    struct hash<hycast::Thread>
    {
        size_t operator()(hycast::Thread& thread) const noexcept
        {
            return hash<std::thread::native_handle_type>()(
                    thread.native_handle());
        }
    };

    template<>
    struct less<hycast::Thread>
    {
        size_t operator()(
                hycast::Thread& thread1,
                hycast::Thread& thread2) const noexcept
        {
            return less<std::thread::native_handle_type>()(
                    thread1.native_handle(),
                    thread2.native_handle());
        }
    };
}

#endif /* MAIN_MISC_THREAD_H_ */
