/**
 * This file declares a RAII class for an independent thread-of-execution that
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
#include <cassert>
#include <condition_variable>
#include <exception>
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
public:
    /**
     * Key for accessing a user-supplied thread-specific pointer.
     * @tparam T  Type of pointed-to value.
     */
    template<class T> class PtrKey
    {
        pthread_key_t   key;

    public:
        /**
         * Constructs.
         * @throw SystemError  `::pthread_key_create()` failed
         */
        PtrKey()
            : key{}
        {
            auto status = ::pthread_key_create(&key, nullptr);
            if (status)
                throw SystemError(__FILE__, __LINE__,
                        "pthread_key_create() failure", status);
        }

        /**
         * Sets the thread-specific pointer.
         * @param ptr  Pointer value. May be `nullptr`
         * @see   get()
         */
        void set(T* ptr)
        {
            ::pthread_setspecific(key, ptr);
        }

        /**
         * Returns the thread-specific pointer. Will be `nullptr` if `set()`
         * hasn't been called.
         * @return  Pointer
         * @see     set()
         */
        T* get()
        {
            return static_cast<T*>(::pthread_getspecific(key));
        }
    };

    /**
     * Key for accessing a user-supplied thread-specific value.
     * @tparam T  Type of value. Must have a copy constructor.
     */
    template<class T> class ValueKey
    {
        pthread_key_t      key;

        static void deletePtr(void* ptr)
        {
            delete static_cast<std::shared_ptr<T>*>(ptr);
        }

    public:
        /**
         * Constructs.
         * @throw SystemError  `::pthread_key_create()` failed
         */
        ValueKey()
            : key{}
        {
            auto status = ::pthread_key_create(&key, &deletePtr);
            if (status)
                throw SystemError(__FILE__, __LINE__,
                        "pthread_key_create() failure", status);
        }

        /**
         * Sets the value. The value is copied.
         * @param value  Value
         * @see get()
         */
        void set(const T& value)
        {
            auto ptr = static_cast<std::shared_ptr<T>*>(
                    ::pthread_getspecific(key));
            if (ptr == nullptr)
                ptr = new std::shared_ptr<T>{};
            *ptr = std::make_shared<T>(value);
            ::pthread_setspecific(key, ptr);
        }

        /**
         * Returns a copy of the value.
         * @return            Copy of the value
         * @throw LogicError  `set()` hasn't been called
         * @see set()
         */
        T get()
        {
            auto ptr = static_cast<std::shared_ptr<T>*>(::pthread_getspecific(key));
            if (ptr == nullptr)
                throw LogicError(__FILE__, __LINE__, "Key is not set");
            return *(*ptr).get();
        }
    };

    class Barrier
    {
        pthread_barrier_t barrier;

    public:
        Barrier(const int numThreads);

        Barrier(const Barrier& that) =delete;
        Barrier(Barrier&& that) =delete;
        Barrier& operator=(const Barrier& rhs) =delete;
        Barrier& operator=(Barrier&& rhs) =delete;

        void wait();

        ~Barrier() noexcept;
    };

private:
    class Impl final
    {
        typedef enum {
            unset = 0,
            completed = 1,
            beingJoined = 2,
            joined = 4
        }                               State;
        typedef std::mutex              Mutex;
        typedef std::lock_guard<Mutex>  LockGuard;
        typedef std::unique_lock<Mutex> UniqueLock;
        typedef std::condition_variable Cond;

        mutable Mutex       mutex;
        mutable Cond        cond;
        std::exception_ptr  exception;
        std::atomic_uint    state;
        Barrier             barrier;
        mutable std::thread stdThread;

        /**
         * Thread cleanup routine for when the thread-of-control is canceled.
         * @param[in] arg  Pointer to an instance of this class.
         */
        static void setCompleted(void* arg);

        /**
         * Ensures that the thread-of-execution associated with this instance
         * has been joined. Idempotent.
         */
        void privateJoin();

        /**
         * Unconditionally cancels the thread-of-execution associated with this
         * instance.
         */
        void privateCancel();

        /**
         * Ensures that the thread-of-execution has completed by canceling it if
         * necessary. Sets the `State::completed` bit in `state`.
         */
        void ensureCompleted();

        /**
         * Ensures that the thread-of-execution is joined. Sets the
         * `State::completed` bit in `state`.
         */
        void ensureJoined();

        /**
         * Indicates whether or not the mutex is locked. Upon return, the state
         * of the mutex is the same as upon entry.
         * @retval `true`    Iff the mutex is locked
         */
        bool isLocked() const;

    public:
        /// Thread identifier
        typedef std::thread::id ThreadId;

        Impl() =delete;
        Impl(const Impl& that) = delete;
        Impl(Impl&& that) =delete;
        Impl& operator=(const Impl& rhs) =delete;
        Impl& operator=(Impl&& rhs) =delete;

        /**
         * Constructs. Either `callable()` will be called or `terminate()` will
         * be called.
         * @param[in] callable  Object to be executed by calling it's
         *                      `operator()` function
         * @param[in] args      Arguments of the `operator()` method
         */
        template<class Callable, class... Args>
        explicit Impl(Callable& callable, Args&&... args)
            : mutex{}
            , cond{}
            , exception{}
            , state{State::unset}
            , barrier{2}
            , stdThread{[this](decltype(
                    std::bind(callable, std::forward<Args>(args)...))&&
                            boundCallable) mutable {
                THREAD_CLEANUP_PUSH(setCompleted, this);
                try {
                    // Ensure deferred cancelability
                    int  previous;
                    auto status = ::pthread_setcanceltype(
                            PTHREAD_CANCEL_DEFERRED, &previous);
                    if (status)
                        throw SystemError(__FILE__, __LINE__,
                                "pthread_setcanceltype() failure", status);
                    try {
                        barrier.wait();
                        Thread::enableCancel();
                        Thread::testCancel(); // In case destructor called
                        boundCallable();
                        Thread::testCancel(); // In case destructor called
                        Thread::disableCancel(); // Disables Thread::testCancel()
                        LockGuard lock{mutex};
                        state.fetch_or(State::completed);
                    }
                    catch (const std::exception& e) {
                        Thread::disableCancel();
                        std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                                "Task threw an exception"));
                    }
                }
                catch (const std::exception& e) {
                    Thread::disableCancel();
                    exception = std::current_exception();
                    LockGuard lock{mutex};
                    state.fetch_or(State::completed);
                    throw;
                }
                THREAD_CLEANUP_POP(false);
            }, std::bind(callable, std::forward<Args>(args)...)}
        {
            barrier.wait();
        }

        /**
         * Destroys. The associated thread-of-execution is canceled if it hasn't
         * completed and joined if it hasn't been.
         */
        ~Impl();

        /**
         * Returns the thread identifier.
         * @return                  Thread identifier
         * @throw  InvalidArgument  This instance was canceled
         * @exceptionsafety         Nothrow
         * @threadsafety            Safe
         */
        ThreadId id() const noexcept;

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
    typedef Impl::ThreadId             Id;

private:
    class ThreadMap
    {
        typedef std::mutex             Mutex;
        typedef std::lock_guard<Mutex> LockGuard;
        typedef std::map<Id, Impl*>    Map;

        Mutex mutex;
        Map   threads;

    public:
        ThreadMap();
        void add(Impl* impl);
        bool contains(const Id& threadId);
        Impl* get(const Id& threadId);
        void erase(const Id& threadId);
        Map::size_type size();
    };

    typedef std::shared_ptr<Impl>      Pimpl;

    /// Set of joinable threads:
    static ThreadMap                   threads;
    /// Pointer to the implementation:
    Pimpl                              pImpl;

    /**
     * Checks invariants.
     * @invariant        `pImpl` <=> thread's ID isn't default
     * @invariant        `Thread` object is joinable <=> its ID isn't default
     * @invariant        `Thread` object is in `threads` <=> it's joinable
     * @exceptionsafety  Strong guarantee
     */
    void checkInvariants() const;

    /**
     * Adds a thread object to the set of such objects.
     * @param[in] thread  Thread object to be added
     */
    void add();

public:
    friend class std::hash<Thread>;
    friend class std::less<Thread>;

    /**
     * Default constructs. The resulting instance will not be joinable.
     */
    Thread();

    /**
     * Copy constructs.
     * @param[in] that  Other instance
     */
    Thread(const Thread& that) =delete;

    /**
     * Move constructs.
     * @param[in] that  Rvalue instance
     */
    Thread(Thread&& that);

    /**
     * Constructs. The resulting instance will be joinable.
     * @param[in] callable  Object to be executed by calling it's
     *                      `operator()` method
     * @param[in] args      Arguments of the `operator()` method
     */
    template<class Callable, class... Args>
    explicit Thread(Callable&& callable, Args&&... args)
        : pImpl{new Impl(callable, std::forward<Args>(args)...)}
    {
        add();
    }

    /**
     * Destroys. Because this is a RAII class, the associated
     * thread-of-execution is canceled if it hasn't completed and joined if it
     * hasn't been joined.
     */
    ~Thread() noexcept;

    /**
     * Copy assigns.
     * @param[in] rhs  Right-hand-side of assignment operator
     * @return         This instance
     */
    Thread& operator=(const Thread& rhs);

    /**
     * Move assigns.
     * @param[in] rhs  Rvalue instance
     * @return         This instance
     */
    Thread& operator=(Thread&& rhs);

    /**
     * Returns the identifier of the current thread. Non-joinable instances
     * all return the same identifier.
     * @return           Identifier of the current thread
     * @exceptionsafety  Nothrow
     * @threadsafety     Safe
     */
    static Id getId() noexcept;

    /**
     * Returns the identifier of this instance.
     * @return                 Identifier of this instance
     * @throw InvalidArgument  This instance was default constructed or joined
     * @exceptionsafety        Strong guarantee
     * @threadsafety           Safe
     */
    Id id() const;

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
     * Disables cancelability of the current thread. Disables `testCancel()`.
     * @return             Previous state
     * @throw SystemError  Cancelability couldn't be set
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     * @see testCancel()
     */
    inline static bool disableCancel()
    {
        return enableCancel(false);
    }

    /**
     * Cancellation point for the current thread. Disabled by `disableCancel()`.
     * @see disableCancel()
     */
    static void testCancel();

    /**
     * Cancels this thread. Idempotent. Does nothing if the thread isn't
     * joinable.
     * @throw SystemError  Thread couldn't be canceled
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    void cancel();

    /**
     * Cancels a thread. Idempotent. Does nothing if the thread isn't joinable.
     * @throw OutOfRange   Thread doesn't exist
     * @throw SystemError  Thread couldn't be canceled
     * @threadsafety       Safe
     */
    static void cancel(const Id& threadId);

    /**
     * Indicates if this instance is joinable.
     * @retval `true`  Iff this instance is joinable
     */
    bool joinable() const noexcept;

    /**
     * Joins this thread. Idempotent. If the thread wasn't canceled, then this
     * call is necessary to ensure that all thread cleanup routines have
     * completed. Does nothing if the thread isn't joinable.
     * @throw SystemError  Thread couldn't be joined
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    void join();

    /**
     * Returns the number of joinable threads.
     * @return Number of joinable threads
     */
    static size_t size();
};

} // namespace

namespace std {
    template<> struct hash<hycast::Thread>
    {
        size_t operator()(hycast::Thread& thread) const noexcept
        {
            return hash<hycast::Thread::Id>()(thread.id());
        }
    };

    template<> struct less<hycast::Thread>
    {
        size_t operator()(
                hycast::Thread& thread1,
                hycast::Thread& thread2) const noexcept
        {
            return less<hycast::Thread::Id>()(thread1.id(), thread2.id());
        }
    };
}

#endif /* MAIN_MISC_THREAD_H_ */
