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

/******************************************************************************/

class Cue
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    Cue();
    void cue() const;
    /**
     * NB: A cancellation point.
     */
    void wait() const;
};

/******************************************************************************/

class Barrier
{
    pthread_barrier_t barrier;

public:
    Barrier(const int numThreads);

    Barrier(const Barrier& that) =delete;
    Barrier(Barrier&& that) =delete;
    Barrier& operator=(const Barrier& rhs) =delete;
    Barrier& operator=(Barrier&& rhs) =delete;

    /**
     * Blocks until the number of threads given to the constructor have called
     * this function, then resets. NB: Not a cancellation point.
     * @throws SystemError  `pthread_barrier_wait()` failure
     */
    void wait();

    ~Barrier() noexcept;
};

/******************************************************************************/

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
                throw SYSTEM_ERROR("pthread_key_create() failure", status);
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
                throw SYSTEM_ERROR("pthread_key_create() failure", status);
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
                throw LOGIC_ERROR("Key is not set");
            return *(*ptr).get();
        }
    };

private:
    class Impl final
    {
    public:
        /// Thread identifier
        typedef std::thread::id ThreadId;

    private:
        class ThreadMap {
            typedef std::mutex                Mutex;
            typedef std::lock_guard<Mutex>    LockGuard;
            typedef std::map<ThreadId, Impl*> Map;

            mutable Mutex mutex;
            Map           threads;

        public:
            ThreadMap();
            void add(Impl* impl);
            bool contains(const ThreadId& threadId);
            Impl* get(const ThreadId& threadId);
            Map::size_type erase(const ThreadId& threadId);
            Map::size_type size();
        };
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
        unsigned            state;
        Barrier             barrier;
        mutable std::thread stdThread;
        std::thread::native_handle_type native_handle;
        /// Set of joinable threads:
        static ThreadMap    threads;

        /**
         * Indicates if the mutex is locked or not.
         *
         * NB: Only assert this in the affirmative (i.e., `assert(isLocked());`
         * because the negative can't be known a priori in a multi-threaded
         * environment.
         *
         * @retval `true`   The mutex is locked
         * @retval `false`  The mutex is not locked
         */
        bool isLocked() const;

        /**
         * Adds a joinable thread object to the set of such objects.
         * @param[in] thread  Joinable thread object to be added
         */
        void add();

        inline void setStateBit(const State bit)
        {
            assert(isLocked());
            state |= bit;
            cond.notify_all(); // Notify of all state changes
        }

        inline void clearStateBit(const State bit)
        {
            assert(isLocked());
            state &= ~bit;
            cond.notify_all(); // Notify of all state changes
        }

        inline bool isStateBitSet(const State bit) const
        {
            assert(isLocked());
            return (state & bit) != 0;
        }

        inline void setCompleted()
        {
            setStateBit(State::completed);
        }

        inline bool isCompleted() const
        {
            return isStateBitSet(State::completed);
        }

        inline void setBeingJoined()
        {
            setStateBit(State::beingJoined);
        }

        inline bool isBeingJoined() const
        {
            return isStateBitSet(State::beingJoined);
        }

        inline void setJoined()
        {
            setStateBit(State::joined);
        }

        inline bool isJoined() const
        {
            return isStateBitSet(State::joined);
        }

        /**
         * Indicates if this instance is joinable or not. The mutex must be
         * locked.
         * @return `true`   Instance is joinable
         * @return `false`  Instance is not joinable
         */
        bool lockedJoinable() const noexcept;

        /**
         * Thread cleanup routine for when the thread-of-execution is canceled.
         * @param[in] arg  Pointer to an instance of this class.
         */
        static void setCompletedAndNotify(void* arg);

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

        static void clearBeingJoined(void* arg) noexcept;

    public:
        Impl() =delete;
        Impl(const Impl& that) = delete;
        Impl(Impl&& that) =delete;
        Impl& operator=(const Impl& rhs) =delete;
        Impl& operator=(Impl&& rhs) =delete;

        /**
         * Constructs. Either `callable()` will be called or `terminate()` will
         * be called. The cancellation state of the resulting thread will be
         * disabled and deferred. The RAII class `Canceler` should be used
         * around statements that might block and for which thread cancellation
         * is desired.
         *
         * @param[in] callable  Object to be executed by calling it's
         *                      `operator()` function
         * @param[in] args      Arguments of the `operator()` method
         * @see `Canceler`
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
                THREAD_CLEANUP_PUSH(setCompletedAndNotify, this);
                try {
                    Thread::disableCancel();
                    // Ensure deferred cancelability when enabled
                    int  previous;
                    ::pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &previous);
                    barrier.wait();
                    try {
                        boundCallable();
                    }
                    catch (const std::exception& e) {
                        std::throw_with_nested(RUNTIME_ERROR(
                                "Task threw an exception"));
                    }
                }
                catch (const std::exception& e) {
                    exception = std::current_exception();
                }
                THREAD_CLEANUP_POP(true);
            }, std::bind(callable, std::forward<Args>(args)...)}
        {
            barrier.wait();
            if (exception)
                std::rethrow_exception(exception);
            native_handle = stdThread.native_handle();
            threads.add(this);
        }

        /**
         * Destroys. The associated thread-of-execution is canceled if it hasn't
         * completed and joined if it hasn't been.
         */
        ~Impl() noexcept;

        /**
         * Returns the thread identifier.
         * @return                  Thread identifier
         * @throw  InvalidArgument  This instance was canceled
         * @exceptionsafety         Nothrow
         * @threadsafety            Safe
         */
        ThreadId id() const noexcept;

        /**
         * Returns the identifier of the current thread. Non-joinable instances
         * all return the same identifier.
         * @return           Identifier of the current thread
         * @exceptionsafety  Nothrow
         * @threadsafety     Safe
         */
        static ThreadId getId() noexcept;

        /**
         * Cancels the thread. Idempotent. The completion of the
         * thread-of-execution is asynchronous with respect to this function.
         * @throw SystemError  Thread couldn't be canceled
         * @exceptionsafety    Strong guarantee
         * @threadsafety       Safe
         */
        void cancel();

        /**
         * Cancels a thread. Idempotent. Does nothing if the thread isn't
         * joinable. The completion of the thread-of-execution is asynchronous
         * with respect to this function.
         * @throw OutOfRange   Thread doesn't exist
         * @throw SystemError  Thread couldn't be canceled
         * @threadsafety       Safe
         */
        static void cancel(const ThreadId& threadId);

        /**
         * Indicates if this instance is joinable or not.
         * @return `true`   Instance is joinable
         * @return `false`  Instance is not joinable
         */
        bool joinable() const;

        /**
         * Joins the thread. Idempotent.
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

public:
    /// Thread identifier
    typedef Impl::ThreadId             Id;

private:
    typedef std::shared_ptr<Impl>      Pimpl;

    /// Pointer to the implementation:
    Pimpl                              pImpl;

public:
    friend class std::hash<Thread>;
    friend class std::less<Thread>;

    /**
     * Default constructs. The resulting instance will not be joinable.
     */
    Thread();

    /**
     * Copy constructs. This constructor is deleted because the compiler
     * confuses it with `Thread(Callable, ...)`.
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
     * @see                 ~Thread
     * @guarantee           The associated thread-of-execution will have been
     *                      joined when the destructor returns
     */
    template<class Callable, class... Args>
    explicit Thread(Callable&& callable, Args&&... args)
        : pImpl{new Impl(callable, std::forward<Args>(args)...)}
    {}

    /**
     * Destroys. Because this is a RAII class, the associated
     * thread-of-execution -- if it exists -- will be canceled if it hasn't
     * completed and joined if it hasn't been joined.
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
     * @retval    `true`   Cancelability was enabled
     * @retval    `false`  Cancelability was not enabled
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    static bool enableCancel(const bool enable = true) noexcept;

    /**
     * Disables cancelability of the current thread. Disables `testCancel()`.
     * @retval    `true`   Cancelability was enabled
     * @retval    `false`  Cancelability was not enabled
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     * @see testCancel()
     */
    inline static bool disableCancel() noexcept
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
     * joinable. The completion of the thread-of-execution is asynchronous with
     * respect to this function.
     * @throw SystemError  Thread couldn't be canceled
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    void cancel();

    /**
     * Cancels a thread. Idempotent. Does nothing if the thread isn't joinable.
     * The completion of the thread-of-execution is asynchronous with respect to
     * this function.
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
     * Joins this thread. Idempotent. Calling this function is necessary to
     * ensure that all thread cleanup routines have completed. Does nothing if
     * the thread isn't joinable.
     * @throw SystemError  Thread couldn't be joined
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    void join();

    /**
     * Returns the number of joinable threads.
     * @return           Number of joinable threads
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    static size_t size();
};

/**
 * RAII class that enables thread cancellation on construction and returns it
 * to its previous state on destruction.
 */
class Canceler
{
    const bool previous;

public:
    /**
     * Enables thread cancellation.
     */
    inline Canceler()
        : previous{Thread::enableCancel(true)}
    {}

    /**
     * Returns thread cancellation to previous state.
     */
    inline ~Canceler()
    {
        Thread::enableCancel(previous);
    }
};

} // namespace

/******************************************************************************/

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
