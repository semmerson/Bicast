/**
 * RAII thread.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: RaiiThread.h
 *  Created on: Jul 27, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_RAIITHREAD_H_
#define MAIN_MISC_RAIITHREAD_H_

#include <memory>

namespace hycast {

class RaiiThread
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    RaiiThread(Impl* const impl);

public:
    /**
     * Default constructs.
     */
    RaiiThread();

    /**
     * Constructs. The resulting thread will be cancelled just before the
     * thread-object referenced by the implementation is destroyed.
     *
     * @tparam Callable  Callable object
     * @tparam Args      Arguments to callable object
     */
    template<class Callable, class... Args>
    RaiiThread(Callable&& callable, Args&&... args);

    /**
     * Destroys.
     */
    ~RaiiThread() noexcept;
};

} // namespace

#endif /* MAIN_MISC_RAIITHREAD_H_ */
