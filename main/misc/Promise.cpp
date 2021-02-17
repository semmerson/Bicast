/**
 * This file implements the shared-state between an asynchronous task and its
 * future.
 *
 *        File: Promise.cpp
 *  Created on: Jun 1, 2017
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

#include "config.h"

#include "error.h"
#include "Promise.h"

#include <condition_variable>
#include <exception>
#include <mutex>

namespace hycast {

class BasicPromise::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;

protected:
    mutable Mutex                   mutex;
    mutable std::condition_variable cond;
    std::exception_ptr              exception;
    bool                            canceled;
    bool                            haveResult;

    /**
     * Indicates if the task is done.
     * @retval `true`   Iff task is done
     * @threadsafety    Compatible but not safe
     */
    bool done() const
    {
        return exception || canceled || haveResult;
    }

    /**
     * Waits until the task is done.
     * @param[in] lock  Already locked mutex
     * @threadsafety    Safe
     */
    void wait(UniqueLock& lock) const
    {
        while (!done()) {
            Canceler canceler{};
            cond.wait(lock);
        }
    }

public:
    Impl()
        : mutex{}
        , cond{}
        , exception{}
        , canceled{false}
        , haveResult{false}
    {}

    void setException()
    {
        LockGuard lock{mutex};
        exception = std::current_exception();
        cond.notify_one();
    }

    void markCanceled()
    {
        LockGuard lock{mutex};
        canceled = true;
        cond.notify_one();
    }

    bool wasCanceled() const
    {
        LockGuard lock{mutex};
        return canceled;
    }

    bool isDone() const
    {
        LockGuard lock{mutex};
        return done();
    }

    void markResult()
    {
        LockGuard lock{mutex};
        haveResult = true;
        cond.notify_one();
    }

    void wait() const
    {
        UniqueLock lock{mutex};
        wait(lock);
    }

    void checkResult() const
    {
        UniqueLock lock(mutex);
        wait(lock);
        if (exception)
            std::rethrow_exception(exception);
        if (canceled)
            throw LogicError("Asynchronous task was cancelled");
    }
};

BasicPromise::BasicPromise(Impl* ptr)
    : pImpl{ptr}
{}

void BasicPromise::setException() const
{
    pImpl->setException();
}

void BasicPromise::markCanceled() const
{
    pImpl->markCanceled();
}

bool BasicPromise::wasCanceled() const
{
    return pImpl->wasCanceled();
}

bool BasicPromise::isDone() const
{
    return pImpl->isDone();
}

void BasicPromise::wait() const
{
    return pImpl->wait();
}

/******************************************************************************/

template<class Ret>
class Promise<Ret>::Impl : public BasicPromise::Impl
{
    Ret result;

public:
    Impl()
        : result{}
    {}

    void setResult(Ret result)
    {
        this->result = result;
        markResult();
    }

    Ret getResult() const
    {
        checkResult();
        return result;
    }
};

template<class Ret>
Promise<Ret>::Promise()
    : BasicPromise{new Impl()}
{}

template<class Ret>
void Promise<Ret>::setResult(Ret result) const
{
    reinterpret_cast<Impl*>(pImpl.get())->setResult(result);
}

template<class Ret>
Ret Promise<Ret>::getResult() const
{
    return reinterpret_cast<Impl*>(pImpl.get())->getResult();
}

template class Promise<int>;

/******************************************************************************/

class Promise<void>::Impl : public BasicPromise::Impl
{
public:
    Impl()
    {}

    void setResult()
    {
        markResult();
    }

    void getResult() const
    {
        checkResult();
    }
};

Promise<void>::Promise()
    : BasicPromise{new Impl()}
{}

void Promise<void>::setResult() const
{
    reinterpret_cast<Impl*>(pImpl.get())->setResult();
}

void Promise<void>::getResult() const
{
    reinterpret_cast<Impl*>(pImpl.get())->getResult();
}

template class Promise<void>;

} // namespace
