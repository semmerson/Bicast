/**
 * Pool of threads for executing peers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerThreadPool.cpp
 *  Created on: Aug 8, 2019
 *      Author: Steven R. Emmerson
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

        bool take(Peer peer) {
            Lock lock{mutex};

            while (!done && !peerIsSet)
                readCond.wait(lock);

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
        Lockout*    lockout;
        Peer        peer;
        bool        peerSet;
        Mutex       mutex;
        std::thread thread;

        void operator()() {
            try {
                Peer tmpPeer{};

                while (lockout->take(tmpPeer)) {
                    {
                        Guard guard{mutex};
                        peer = tmpPeer;
                        peerSet = true;
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
                    }

                    lockout->incNumIdle();
                }
            }
            catch (const std::exception& ex) {
                log_error(ex);
            }
        }

    public:
        Worker()
            : lockout{nullptr}
            , peer()
            , peerSet{false}
            , mutex()
            , thread{}
        {}

        Worker(Lockout* lockout)
            : lockout{lockout}
            , peer()
            , peerSet{false}
            , mutex()
            , thread(&Worker::operator(), this)
        {}

        ~Worker() noexcept {
            if (thread.joinable()) {
                bool terminatePeer;
                {
                    Guard guard(mutex);
                    terminatePeer = peerSet;
                }
                if (terminatePeer)
                    peer.terminate(); // Idempotent

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
