/**
 * This file implements the future of an asynchronous task.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Future.cpp
 * @author: Steven R. Emmerson
 */

#include "Future.h"
#include "FutureImpl.h"

namespace hycast {

template<class R>
Future<R>::Future()
    : pImpl()
{}

template<class R>
Future<R>::Future(std::function<R()> func)
    : pImpl(new FutureImpl<R>(func))
{}

template<class R>
bool Future<R>::operator==(const Future<R>& that)
{
    return pImpl == that.pImpl;
}

template<class R>
bool Future<R>::operator<(const Future<R>& that) noexcept
{
    return pImpl < that.pImpl;
}

template<class R>
void Future<R>::execute() const
{
    pImpl->execute();
}

template<class R>
pthread_t Future<R>::getThreadId() const
{
    return pImpl->getThreadId();
}

template<class R>
void Future<R>::cancel() const
{
    pImpl->cancel();
}

template<class R>
void Future<R>::wait() const
{
    pImpl->wait();
}

template<class R>
bool Future<R>::wasCancelled() const
{
    return pImpl->wasCancelled();
}

template<class R>
R Future<R>::getResult() const
{
    return pImpl->getResult();
}

Future<void>::Future()
    : pImpl()
{}

Future<void>::Future(std::function<void()> func)
    : pImpl(new FutureImpl<void>(func))
{}

bool Future<void>::operator==(const Future<void>& that)
{
    return pImpl == that.pImpl;
}

bool Future<void>::operator<(const Future<void>& that) noexcept
{
    return pImpl < that.pImpl;
}

void Future<void>::execute() const
{
    pImpl->execute();
}

pthread_t Future<void>::getThreadId() const
{
    return pImpl->getThreadId();
}

void Future<void>::cancel() const
{
    pImpl->cancel();
}

void Future<void>::wait() const
{
    pImpl->wait();
}

bool Future<void>::wasCancelled() const
{
    return pImpl->wasCancelled();
}

void Future<void>::getResult() const
{
    pImpl->getResult();
}

//template class Future<>;
template class Future<int>;
template class Future<void>;

} // namespace
