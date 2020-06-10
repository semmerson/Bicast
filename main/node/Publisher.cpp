/**
 * Publisher node.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Publisher.cpp
 *  Created on: Jan 3, 2020
 *      Author: Steven R. Emmerson
 */

#include <node/Publisher.h>
#include "config.h"

#include "error.h"
#include "McastProto.h"
#include "P2pMgr.h"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace hycast {

class Publisher::Impl : public SendNode
{
    typedef std::mutex              Mutex;
    typedef std::condition_variable Cond;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;

    Mutex              mutex;
    Cond               cond;
    P2pMgr             p2pMgr;
    McastSndr          mcastSndr;
    PubRepo            repo;
    SegSize            segSize;
    std::exception_ptr exPtr; ///< Subtask exception
    std::thread        p2pThread;
    std::thread        sendThread;
    bool               done;

    /**
     * Sends product-information.
     *
     * @param[in] prodInfo  Product-information to be sent
     */
    void send(const ProdInfo& prodInfo)
    {
        mcastSndr.multicast(prodInfo);
        p2pMgr.notify(prodInfo.getProdIndex());
    }

    /**
     * Sends a data-segment
     *
     * @param[in] memSeg  Data-segment to be sent
     */
    void send(const MemSeg& memSeg)
    {
        mcastSndr.multicast(memSeg);
        p2pMgr.notify(memSeg.getSegId());
    }

    /**
     * Executes the P2P manager.
     */
    void runP2p()
    {
        try {
            p2pMgr();
        }
        catch (const std::exception& ex) {
            Guard guard(mutex);
            exPtr = std::make_exception_ptr(ex);
            cond.notify_one();
            // Not re-thrown so `std::terminate()` isn't called
        }
        catch (...) {
            // Cancellation eaten so `std::terminate()` isn't called
        }
    }

    /**
     * Executes the sender of new data-products in the repository.
     */
    void runSender()
    {
        try {
            for (;;) {
                auto prodIndex = repo.getNextProd();

                // Send product-information
                auto prodInfo = repo.getProdInfo(prodIndex);
                // TODO: Test for valid `prodInfo`
                send(prodInfo);

                // Send data-segments
                auto prodSize = prodInfo.getProdSize();
                for (ProdSize offset = 0; offset < prodSize; offset += segSize)
                    // TODO: Test for valid segment
                    send(repo.getMemSeg(SegId(prodIndex, offset)));
            }
        }
        catch (const std::exception& ex) {
            Guard guard(mutex);
            exPtr = std::make_exception_ptr(ex);
            cond.notify_one();
            // Not re-thrown so `std::terminate()` isn't called
        }
        catch (...) {
            // Cancellation eaten so `std::terminate()` isn't called
        }
    }

public:
    /**
     * Constructs.
     *
     * @param[in] p2pSrvrInfo  Information on publishing P2P server
     * @param[in] grpAddr      Multicast group address
     * @param[in] repoDir      Pathname of root directory of repository
     */
    Impl(   P2pInfo&           p2pSrvrInfo,
            const SockAddr&    grpAddr,
            const std::string& repoDir)
        : mutex()
        , cond()
        , p2pMgr{}
        , mcastSndr{UdpSock(grpAddr)}
        , repo(repoDir)
        , segSize{repo.getSegSize()}
        , exPtr()
        , p2pThread()
        , sendThread()
        , done(false)
    {
        mcastSndr.setMcastIface(p2pSrvrInfo.sockAddr.getInetAddr());

        p2pMgr = PubP2pMgr(p2pSrvrInfo, *this);
    }

    /**
     * Destroys.
     */
    ~Impl() noexcept
    {
        Guard guard(mutex);

        if (sendThread.joinable()) {
            ::pthread_cancel(sendThread.native_handle());
            sendThread.join();
        }

        if (p2pThread.joinable()) {
            p2pMgr.halt();
            p2pThread.join();
        }
    }

    /**
     * Executes this instance. A P2P manager is executed and the repository is
     * watched for new data-products to be sent.
     */
    void operator()()
    {
        p2pThread = std::thread(&Impl::runP2p, this);

        try {
            sendThread = std::thread(&Impl::runSender, this);

            try {
                Lock lock(mutex);

                while (!done && !exPtr)
                    cond.wait(lock);

                if (exPtr)
                    std::rethrow_exception(exPtr);

                ::pthread_cancel(sendThread.native_handle());
                sendThread.join();
            } // Sender thread started
            catch (const std::exception& ex) {
                ::pthread_cancel(sendThread.native_handle());
                sendThread.join();
                throw;
            }

            p2pMgr.halt();
            p2pThread.join();
        } // P2P thread started
        catch (const std::exception& ex) {
            p2pMgr.halt();
            p2pThread.join();
            throw;
        }
    }

    /**
     * Halts this instance.
     */
    void halt()
    {
        Guard guard(mutex);
        done = true;
        cond.notify_one();
    }

    bool shouldRequest(ProdIndex prodIndex)
    {
        return false; // Meaningless for a sender
    }

    bool shouldRequest(const SegId& segId)
    {
        return false; // Meaningless for a sender
    }

    bool hereIsP2p(const ProdInfo& prodInfo)
    {
        return false; // Meaningless for a sender
    }

    bool hereIs(TcpSeg& tcpSeg)
    {
        return false; // Meaningless for a sender
    }

    ProdInfo get(ProdIndex prodIndex)
    {
        return repo.getProdInfo(prodIndex);
    }

    MemSeg get(const SegId& segId)
    {
        return repo.getMemSeg(segId);
    }
};

/******************************************************************************/

Publisher::Publisher(
        P2pInfo&        p2pSrvrInfo,
        const SockAddr& grpAddr,
        std::string&    repoDir)
    : pImpl{new Impl(p2pSrvrInfo,  grpAddr, repoDir)} {
}

void Publisher::operator()() const {
    pImpl->operator()();
}

void Publisher::halt() const {
    pImpl->halt();
}

} // namespace
