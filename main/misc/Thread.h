/**
 * This file declares a RAII class for an independent thread-of-execution that
 * supports copying, assignment, and POSIX thread cancellation.
 *
 *        File: Thread.h
 *  Created on: May 11, 2017
 *      Author: Steven R. Emmerson
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
#include <vector>

#define THREAD_CLEANUP_PUSH(func, arg) pthread_cleanup_push(func, arg)
#define THREAD_CLEANUP_POP(execute)    pthread_cleanup_pop(execute)

namespace bicast {

/******************************************************************************/

/**
 * RAII class for temporarily unlocking a mutex.
 */
class UnlockGuard final
{
    std::mutex& mutex;

public:
    /**
     * Constructs. Unlocks the mutex.
     * @param[in] mutex  Mutex to be unlocked. Must exist for the duration of
     *                   this instance.
     */
    UnlockGuard(std::mutex& mutex)
        : mutex(mutex) // g++ 4.8 doesn't support `mutex{mutex}`; clang does
    {
        mutex.unlock();
    }
    /**
     * Copy constructs
     * @param[in] unlock  The other instance
     */
    UnlockGuard(const UnlockGuard& unlock) =delete;
    /**
     * Destroys. Locks the mutex given to the constructor.
     */
    ~UnlockGuard()
    {
        mutex.lock();
    }
};

/******************************************************************************/

/// A class for blocking a thread until cued
class Cue
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    Cue();
    /**
     * Signals (i.e., "cues") this instance causing `wait()` to return.
     */
    void cue() const;
    /**
     * NB: A cancellation point.
     */
    void wait() const;
};

/******************************************************************************/

/// A class for synchronizing the arrival of multiple threads
class Barrier
{
    pthread_barrier_t barrier;

public:
    /**
     * Constructs.
     * @param[in] numThreads  Number of threads that will access this instance
     */
    Barrier(const int numThreads);

    /**
     * Copy constructs.
     * @param[in] that  The other instance
     */
    Barrier(const Barrier& that) =delete;
    /**
     * Move constructs.
     * @param[in] that  The other instance
     */
    Barrier(Barrier&& that) =delete;
    /**
     * Copy assigns.
     * @param[in] rhs  The other, right-hand-side instance
     * @return         A reference to this, just-assigned instance
     */
    Barrier& operator=(const Barrier& rhs) =delete;
    /**
     * Move assigns.
     * @param[in,out] rhs  The other, right-hand-side instance
     * @return             A reference to this, just-assigned instance
     */
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

#if 0
/// Thread
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
         * @throw SystemError  Couldn't create thread
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
         * @throw SystemError  Couldn't create thread
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

            mutable Mutex      mutex;
            Map                threads;
            std::vector<bool>  threadIndexes;

            long setAndGetLowestUnusedIndex() noexcept;

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

        mutable Mutex                   mutex;
        mutable Cond                    cond;
        std::exception_ptr              exception;
        unsigned                        state;
        Barrier                         barrier;
        long                            threadIndex;
        mutable std::thread             stdThread;
        std::thread::native_handle_type native_handle;
        /// Set of joinable threads:
        static ThreadMap                threads;

        /**
         * Indicates if the mutex is locked or not.
         *
         * NB: Only assert this in the affirmative (i.e., `assert(isLocked());`
         * because the negative can't be known a priori in a multi-threaded
         * environment.
         *
         * @retval true     The mutex is locked
         * @retval false    The mutex is not locked
         */
        bool isLocked() const;

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
         * @return true     Instance is joinable
         * @return false    Instance is not joinable
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
            , threadIndex{-1} // Unknown thread number
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
            threads.add(this); // Sets `this->threadIndex`
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
         * Returns the thread-number. This is useful in,
         * for example, logging to identify messages from the same thread.
         * @return     Thread number
         */
        long threadNumber() const noexcept;

        /**
         * Returns the thread-number of the current thread. This is useful in,
         * for example, logging to identify messages from the same thread.
         * @retval -1  Current thread isn't a `Thread`
         * @return     Thread number of the current thread
         */
        static long getThreadNumber() noexcept;

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
         * @return true     Instance is joinable
         * @return false    Instance is not joinable
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
    typedef Impl::ThreadId             Tag;

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
    static Tag getId() noexcept;

    /**
     * Returns the thread-number of the current thread. This is useful in,
     * for example, logging to identify messages from the same thread.
     * @retval -1  Current thread isn't a `Thread`
     * @return     Thread number of the current thread
     */
    static long getThreadNumber() noexcept;

    /**
     * Returns the identifier of this instance.
     * @return                 Identifier of this instance
     * @throw InvalidArgument  This instance was default constructed or joined
     * @exceptionsafety        Strong guarantee
     * @threadsafety           Safe
     */
    Tag id() const;

    /**
     * Returns the thread-number. This is useful in,
     * for example, logging to identify messages from the same thread.
     * @return     Thread number
     */
    long threadNumber() const noexcept;

    /**
     * Sets the cancelability of the current thread.
     * @param[in] enable   Whether or not to enable cancelability
     * @retval    true     Cancelability was enabled
     * @retval    false    Cancelability was not enabled
     * @exceptionsafety    Strong guarantee
     * @threadsafety       Safe
     */
    static bool enableCancel(const bool enable = true) noexcept;

    /**
     * Disables cancelability of the current thread. Disables `testCancel()`.
     * @retval    true     Cancelability was enabled
     * @retval    false    Cancelability was not enabled
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
    static void cancel(const Tag& threadId);

    /**
     * Indicates if this instance is joinable.
     * @retval true    Iff this instance is joinable
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
#endif

} // namespace

/******************************************************************************/

namespace std {

using namespace bicast;

#if 0
    /// Class function for hashing a thread.
    template<> struct hash<Thread>
    {
        /**
         * Returns the hash code of a thread.
         * @param[in] thread  The thread
         * @return The hash code of a thread
         */
        size_t operator()(Thread& thread) const noexcept
        {
            return hash<Thread::Tag>()(thread.id());
        }
    };

    /// The less-than class function for a thread
    template<> struct less<Thread>
    {
        /**
         * Indicates if one thread is less than another.
         * @param[in] lhs      The left-hand-side thread
         * @param[in] rhs      The right-hand-side thread
         * @retval    true     The left-hand-side is less than the right-hand-side
         * @retval    false    The left-hand-side is not less than the right-hand-side
         */
        size_t operator()(
                Thread& lhs,
                Thread& rhs) const noexcept
        {
            return less<Thread::Tag>()(lhs.id(), rhs.id());
        }
    };
#else
    /// Class function for hashing a thread.
    template<> struct hash<thread>
    {
        /**
         * Returns the hash code of a thread.
         * @param[in] thread  The thread
         * @return The hash code of a thread
         */
        size_t operator()(thread& thread) const noexcept
        {
            return hash<thread::id>()(thread.get_id());
        }
    };

    /// The less-than class function for a thread
    template<> struct less<thread>
    {
        /**
         * Indicates if one thread is less than another.
         * @param[in] lhs      The left-hand-side thread
         * @param[in] rhs      The right-hand-side thread
         * @retval    true     The left-hand-side is less than the right-hand-side
         * @retval    false    The left-hand-side is not less than the right-hand-side
         */
        size_t operator()(
                thread& lhs,
                thread& rhs) const noexcept
        {
            return less<thread::id>()(lhs.get_id(), rhs.get_id());
        }
    };
#endif
}

#endif /* MAIN_MISC_THREAD_H_ */
