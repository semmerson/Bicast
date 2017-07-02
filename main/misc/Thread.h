/**
 * This file declares a RAII class for an independent thread-of-control that
 * supports copying, assignment, and POSIX thread cancellation.
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
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <pthread.h>
#include <thread>

#define THREAD_CLEANUP_PUSH(func, arg) pthread_cleanup_push(func, arg)
#define THREAD_CLEANUP_POP(execute)    pthread_cleanup_pop(execute)

namespace hycast {

class Thread final
{
private:
    class Impl final
    {
    private:
        std::atomic_bool  done;
        std::thread       stdThread;

        /**
         * Ensures that the thread-of-control associated with this instance has
         * been joined.
         */
        void ensureJoined();

        /**
         * Unconditionally cancels the thread-of-control associated with this
         * instance.
         */
        void privateCancel();

    public:
        /// Thread identifier
        typedef std::thread::id ThreadId;

        /**
         * Constructs.
         * @param[in] callable  Object to be executed by calling it's
         *                      `operator()` method
         * @param[in] args      Arguments of the `operator()` method
         */
        template<class Callable, class... Args>
        explicit Impl(Callable& callable, Args&&... args)
            : done{false}
            // Ensure deferred cancelability
            , stdThread{[this](decltype(
                      std::bind(callable, std::forward<Args>(args)...))&&
                            boundCallable)
                      mutable {
                  try {
                      try {
                          int previous;
                          if (::pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,
                                  &previous))
                              throw SystemError(__FILE__, __LINE__,
                                      "pthread_setcanceltype() failure");
                          Thread::enableCancel();
                      }
                      catch (const std::exception& e) {
                          log_what(e, __FILE__, __LINE__,
                                  "Couldn't start thread");
                      }
                      boundCallable();
                      done = true;
                  }
                  catch (const std::exception& e) {
                      done = true;
                      throw;
                  }
                  catch (...) {
                      //log_log("Cancellation exception");
                      throw;
                  }
              }, std::bind(callable, std::forward<Args>(args)...)}
        {}

        /**
         * Destroys. The associated thread-of-control is canceled and joined
         * if necessary.
         */
        ~Impl();

        /**
         * Returns the thread identifier.
         * @return                  Thread identifier
         * @throw  InvalidArgument  This instance was canceled
         * @exceptionsafety         Strong guarantee
         * @threadsafety            Safe
         */
        ThreadId id() const;

        /**
         * Cancels the thread. Idempotent.
         * @throw SystemError  Thread couldn't be canceled
         * @exceptionsafety    Strong guarantee
         * @threadsafety       Safe
         */
        void cancel();

        /**
         * Joins the thread. Idempotent. This call is necessary to ensure that
         * all thread cleanup routines have completed.
         * @throw SystemError  Thread couldn't be joined
         * @exceptionsafety    Strong guarantee
         * @threadsafety       Safe
         */
        void join();
    };

public:
    /// Thread identifier
    typedef Impl::ThreadId            ThreadId;

private:
    typedef std::mutex                Mutex;
    typedef std::lock_guard<Mutex>    LockGuard;
    typedef std::shared_ptr<Impl>     Pimpl;

    static Mutex                      mutex;
    /**
     * Set of active threads. The mapped-value isn't a `Thread` because that
     * would prevent its destruction because `Thread::pImpl.count()` would never
     * reach zero.
     */
    static std::map<ThreadId, Pimpl>  threads;
    Pimpl                             pImpl;

    /**
     * Adds a thread object to the set of such objects.
     * @param[in] thread  Thread object to be added
     */
    static void add(Thread& thread);

    static void staticCancel(const Pimpl& pImpl);

public:
    friend class std::hash<Thread>;
    friend class std::less<Thread>;

    /**
     * Default constructs.
     */
    Thread();

    /**
     * Copy constructs.
     * @param[in] that  Other object
     */
    Thread(const Thread& that);

    /**
     * Move constructs.
     * @param[in] that  Rvalue object
     */
    Thread(Thread&& that);

    /**
     * Constructs.
     * @param[in] callable  Object to be executed by calling it's
     *                      `operator()` method
     * @param[in] args      Arguments of the `operator()` method
     */
    template<class Callable, class... Args>
    explicit Thread(Callable&& callable, Args&&... args)
        : pImpl{new Impl(callable, std::forward<Args>(args)...)}
    {
        add(*this);
    }

    /**
     * Destroys. Because this is a RAII class, the associated thread-of-control
     * is canceled and joined if necessary.
     */
    ~Thread();

    /**
     * Copy assigns.
     * @param[in] rhs  Right-hand-side of assignment operator
     * @return         This instance
     */
    Thread& operator=(const Thread& rhs);

    //Thread& operator=(Thread&& rhs);

    /**
     * Returns the identifier of the current thread.
     * @return           Identifier of the current thread
     * @exceptionsafety  Nothrow
     * @threadsafety     Safe
     */
    static ThreadId getId() noexcept;

    /**
     * Returns the identifier of this instance.
     * @return                 Identifier of this instance
     * @throw InvalidArgument  This instance was default constructed or joined
     * @exceptionsafety        Strong guarantee
     * @threadsafety           Safe
     */
    ThreadId id() const;

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
     * @throw OutOfRange   Thread doesn't exist
     * @throw SystemError  Thread couldn't be canceled
     * @threadsafety       Safe
     */
    static void cancel(const ThreadId& threadId);

    /**
     * Cancels the thread only if it hasn't been default constructed.
     * Idempotent.
     * @throw SystemError  Thread couldn't be canceled
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    void cancel() const;

    /**
     * Joins the thread. Idempotent. This call is necessary to ensure that all
     * thread cleanup routines have completed.
     * @throw SystemError  Thread couldn't be joined
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    void join();

    static size_t size();
};

} // namespace

namespace std {
    template<>
    struct hash<hycast::Thread>
    {
        size_t operator()(hycast::Thread& thread) const noexcept
        {
            return hash<hycast::Thread::ThreadId>()(thread.id());
        }
    };

    template<>
    struct less<hycast::Thread>
    {
        size_t operator()(
                hycast::Thread& thread1,
                hycast::Thread& thread2) const noexcept
        {
            return less<hycast::Thread::ThreadId>()(thread1.id(), thread2.id());
        }
    };
}

#endif /* MAIN_MISC_THREAD_H_ */
