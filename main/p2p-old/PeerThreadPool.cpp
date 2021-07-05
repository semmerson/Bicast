/**
 * Pool of threads for executing peers.
 *
 *        File: PeerThreadPool.cpp
 *  Created on: Aug 8, 2019
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
#include "PeerThreadPool.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace hycast {

class PeerThreadPool::Impl
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;

    class Lockout
    {
        Mutex    mutex;
        Cond     readCond;
        Cond     writeCond;
        Peer     peer;
        bool     peerIsSet;
        bool     done;
        unsigned numIdle;

    public:
        Lockout(const unsigned numWorkers)
            : mutex{}
            , readCond()
            , writeCond()
            , peer()
            , peerIsSet{false}
            , done{false}
            , numIdle{numWorkers}
        {}

        ~Lockout() noexcept
        {}

        bool put(Peer peer) {
            Lock lock{mutex};

            if (numIdle == 0)
                return false;

            while (!done && peerIsSet)
                writeCond.wait(lock);

            if (done)
                return false;

            this->peer = peer;
            peerIsSet = true;
            --numIdle;
            readCond.notify_one();

            return true;
        }

        /**
         * @param[in] peer
         * @return
         * @cancellationpoint
         */
        bool take(Peer peer) {
            Lock lock{mutex};

            while (!done && !peerIsSet)
                readCond.wait(lock); // Cancellation point

            if (done)
                return false;

            peer = this->peer;
            peerIsSet = false;
            writeCond.notify_one();

            return true;
        }

        void incNumIdle() {
            Guard guard{mutex};
            ++numIdle;
        }

        void setDone() {
            Guard guard{mutex};
            done = true;
            readCond.notify_all();
            writeCond.notify_all();
        }
    };

    class Worker {
        Mutex       mutex;
        Cond        cond;
        Lockout*    lockout;
        Peer        peer;
        bool        peerSet;
        bool        done;
        std::thread thread;

        void operator()() {
            try {
                Peer tmpPeer{};

                while (!done && lockout->take(tmpPeer)) { // Cancellation point
                    {
                        Guard guard{mutex};
                        peer = tmpPeer;
                        peerSet = true;
                        cond.notify_one();
                    }

                    try {
                        peer();
                    }
                    catch (const std::exception& ex) {
                        log_error(ex);
                    }

                    {
                        Guard guard(mutex);
                        peerSet = false;
                        cond.notify_one();
                    }

                    lockout->incNumIdle();
                }
            }
            catch (const std::exception& ex) {
                log_error(ex);
            }
        }

        void stop() {
            Lock lock(mutex);

            done = true;

            if (peerSet) {
                peer.halt(); // Idempotent

                while (peerSet)
                    cond.wait(lock);
            }
        }

    public:
        Worker()
            : mutex()
            , cond()
            , lockout{nullptr}
            , peer()
            , peerSet{false}
            , done{true}
            , thread{}
        {}

        Worker(Lockout* lockout)
            : lockout{lockout}
            , peer()
            , peerSet{false}
            , mutex()
            , cond()
            , thread(&Worker::operator(), this)
            , done{false}
        {}

        ~Worker() noexcept {
            if (thread.joinable()) {
                Lock lock(mutex);

                // TODO: Handle thread during destruction
                while

                bool terminatePeer;
                {
                    Guard guard(mutex);
                    terminatePeer = peerSet;
                }
                if (terminatePeer)
                    peer.halt(); // Idempotent

                int status = ::pthread_cancel(thread.native_handle());
                if (status)
                    LOG_ERROR("Couldn't cancel worker thread: %s",
                            ::strerror(status));

                thread.join();
            }
        }

        Worker& operator=(const Worker& rhs) =delete;

        Worker& operator=(Worker&& rhs) {
            lockout = rhs.lockout;
            peer = rhs.peer;
            peerSet = rhs.peerSet;
            thread.swap(rhs.thread);
            return *this;
        }
    };

    Lockout             lockout; ///< Peer execution queue
    std::vector<Worker> workers; ///< Execution worker-threads

public:
    explicit Impl(const size_t numThreads)
        : lockout(numThreads)
        , workers(numThreads)
    {
        auto end = workers.end();

        for (auto iter = workers.begin(); iter != end; ++iter)
            *iter = Worker(&lockout);
    }

    ~Impl() {
        lockout.setDone();
    }

    bool execute(Peer peer) {
        return lockout.put(peer);
    }
};

/******************************************************************************/

PeerThreadPool::PeerThreadPool(const size_t numThreads)
    : pImpl{new Impl(numThreads)} {
}

bool PeerThreadPool::execute(Peer peer) {
    return pImpl->execute(peer);
}

} // namespace
