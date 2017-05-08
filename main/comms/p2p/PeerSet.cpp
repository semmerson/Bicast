/**
 * This file implements a set of peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSet.cpp
 * @author: Steven R. Emmerson
 */

#include "ChunkInfo.h"
#include "ClntSctpSock.h"
#include "error.h"
#include "InetSockAddr.h"
#include "logging.h"
#include "MsgRcvr.h"
#include "Peer.h"
#include "PeerSet.h"
#include "ProdInfo.h"
#include "SyncQueue.h"

#include <assert.h>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include <utility>

namespace hycast {

/**
 * Indicates if a mutex is locked by the current thread.
 * @param[in,out] mutex  The mutex
 * @return `true`        Iff the mutex is locked
 */
bool isLocked(std::mutex& mutex)
{
    if (!mutex.try_lock())
        return true;
    mutex.unlock();
    return false;
}

class PeerSet::Impl final
{
    typedef std::chrono::seconds                               TimeRes;
    typedef std::chrono::time_point<std::chrono::steady_clock> Time;
    typedef std::chrono::steady_clock                          Clock;

    /// An abstract base class for send-actions to be performed on a peer.
    class SendAction
    {
        Time whenCreated;
    public:
        SendAction()
            : whenCreated{Clock::now()}
        {}
        virtual ~SendAction() =default;
        Time getCreateTime() const
        {
            return whenCreated;
        }
        /**
         * Acts upon a peer.
         * @param[in,out] peer  Peer to be acted upon
         * @return `true`       Iff processing should continue
         * @exceptionsafety     No throw
         * @threadsafety        Safe
         */
        virtual void actUpon(Peer& peer) =0;
        virtual bool terminate() const noexcept
        {
            return false;
        }
    };

    /// A send-action notice of a new product.
    class SendProdNotice final : public SendAction
    {
        ProdInfo info;
    public:
        SendProdNotice(const ProdInfo& info)
            : info{info}
        {}
        /**
         * Sends a notice of a data-product to a remote peer.
         * @param[in,out] peer  Peer
         * @exceptionsafety     Basic
         * @threadsafety        Compatible but not safe
         */
        void actUpon(Peer& peer)
        {
            peer.sendNotice(info);
        }
    };

    /// A send-action notice of a new chunk-of-data.
    class SendChunkNotice final : public SendAction
    {
        ChunkInfo info;
    public:
        SendChunkNotice(const ChunkInfo& info)
            : info{info}
        {}
        /**
         * Sends a notice of the availability of a chunk-of-data to a remote
         * peer.
         * @param[in,out] peer  Peer
         * @exceptionsafety     Basic
         * @threadsafety        Compatible but not safe
         */
        void actUpon(Peer& peer)
        {
            peer.sendNotice(info);
        }
    };

    /// A send-action that terminates the peer.
    class TerminatePeer final : public SendAction
    {
    public:
        void actUpon(Peer& peer)
        {}
        bool terminate() const noexcept
        {
            return true;
        }
    };

	/**
	 * A class that adds behaviors to a peer. Specifically, it creates and
	 * manages sending and receiving threads for the peer.
	 */
	class PeerWrapper
	{
		/// A queue of send-actions to be performed on a peer.
		class SendQ final
		{
			std::queue<std::shared_ptr<SendAction>> queue;
			std::mutex                              mutex;
			std::condition_variable                 cond;
			bool                                    isDisabled;
		public:
			SendQ()
				: queue{}
				, mutex{}
				, cond{}
				, isDisabled{false}
			{}

			/// Prevents copy and move construction and assignment
			SendQ(const SendQ& that) =delete;
			SendQ(const SendQ&& that) =delete;
			SendQ& operator =(const SendQ& q) =delete;
			SendQ& operator =(const SendQ&& q) =delete;

			/**
			 * Adds an action to the queue. Entries older than the maximum
			 * residence time will be deleted.
			 * @param[in] action         Action to be added
			 * @param[in] maxResideTime  Maximum residence time
			 * @retval    true           Action was added
			 * @retval    false          Action was not added because
			 *                           `terminate()` was called
			 */
			bool push(
					std::shared_ptr<SendAction> action,
					const TimeRes&              maxResideTime)
			{
				std::lock_guard<decltype(mutex)> lock{mutex};
				if (isDisabled)
					return false;
				while (!queue.empty()) {
					if (Clock::now() - queue.front()->getCreateTime() >
							maxResideTime)
						queue.pop();
				}
				queue.push(action);
				cond.notify_one();
				return true;
			}

			std::shared_ptr<PeerSet::Impl::SendAction> pop()
			{
				std::unique_lock<decltype(mutex)> lock{mutex};
				while (queue.empty())
					cond.wait(lock);
				auto action = queue.front();
				queue.pop();
				return action;
			}

			/**
			 * Clears the queue and adds a send-action that will terminate
			 * the associated peer. Causes `push()` to always fail. Idempotent.
			 */
			void terminate()
			{
				std::unique_lock<decltype(mutex)> lock{mutex};
				while (!queue.empty())
					queue.pop();
				queue.push(std::shared_ptr<SendAction>(new TerminatePeer()));
				isDisabled = true;
				cond.notify_one();
			}
		};

		typedef int32_t            PeerValue;

		SendQ                      sendQ;
		Peer                       peer;
		std::function<void(Peer&)> handleFailure;
		PeerValue                  value;
		std::thread                sendThread;
		std::thread                recvThread;
		std::atomic_bool           reportFailure;

		/**
		 * Reports a failed peer to the failed-peer observer.
		 * @param[in] arg  Pointer to relevant `PeerWrapper` instance
		 */
		static void reportFailedPeer(void* arg) noexcept
		{
			try {
				auto wrapper = static_cast<PeerWrapper*>(arg);
				wrapper->handleFailure(wrapper->peer);
			}
			catch (const std::exception& e) {
				log_what(e); // Because this function mustn't throw
			}
		}

		/**
		 * Processes send-actions queued-up for a peer. Doesn't return unless an
		 * exception is thrown. Intended to run on its own, non-cancellable
		 * thread.
		 */
		void runSender()
		{
			try {
				try {
					for (;;) {
						std::shared_ptr<SendAction> action{sendQ.pop()};
						action->actUpon(peer);
						if (action->terminate())
							break;
					}
				}
				catch (const std::exception& e) {
					std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
							"Can no longer send to remote peer"));
					handleFailure(peer);
				}
			}
			catch (const std::exception& e) {
				log_what(e); // Because end of thread
			}
		}

		/**
		 * Causes a peer to receive messages from its associated remote peer.
		 * Doesn't return until the remote peer disconnects or an exception is
		 * thrown. Intended to run on its own, non-cancellable thread.
		 */
		void runReceiver()
		{
			pthread_cleanup_push(reportFailedPeer, this);
			try {
				try {
					peer.runReceiver();
					handleFailure(peer);
				}
				catch (const std::exception& e) {
					std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
							"Can no longer receive from remote peer"));
					handleFailure(peer);
				}
			}
			catch (const std::exception& e) {
				log_what(e); // Because end of thread
			}
			pthread_cleanup_pop(reportFailure); // Nothrow
		}

	public:
		static const PeerValue VALUE_MAX{INT32_MAX};
		static const PeerValue VALUE_MIN{INT32_MIN};

		/**
		 * Constructs from the peer. Immediately starts receiving and sending.
		 * @param[in] peer           The peer
		 * @param[in] handleFailure  Function to handle failure of peer. Must
		 *                           not throw an exception.
		 * @throw     RuntimeError   Instance couldn't be constructed
		 */
		PeerWrapper(Peer& peer, std::function<void(Peer&)> handleFailure)
			: sendQ{}
			, peer{peer}
			, handleFailure{handleFailure}
			, value{0}
			, sendThread{}
			, recvThread{}
			, reportFailure{true}
		{
			try {
				recvThread = std::thread([=]{runReceiver();});
				sendThread = std::thread([=]{runSender();});
			}
			catch (const std::exception& e) {
				if (recvThread.joinable()) {
					::pthread_cancel(recvThread.native_handle());
					recvThread.join();
				}
				std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
						"Couldn't construct peer-wrapper"));
			}
		}

		/// Prevents copy and move construction and assignment.
		PeerWrapper(const PeerWrapper& that) =delete;
		PeerWrapper(const PeerWrapper&& that) =delete;
		PeerWrapper& operator=(PeerWrapper& rhs) =delete;
		PeerWrapper& operator=(PeerWrapper&& rhs) =delete;

		/// Destroys.
		~PeerWrapper()
		{
			try {
				reportFailure = false;
				sendQ.terminate();
				::pthread_cancel(recvThread.native_handle());
				sendThread.join();
				recvThread.join();
			}
			catch (const std::exception& e) {
				// Destructors must never throw an exception
				log_what(e);
			}
		}

		/**
		 * Increments the value of the peer.
		 * @exceptionsafety Strong
		 * @threadsafety    Safe
		 */
		void incValue()
		{
			if (value < VALUE_MAX)
				++value;
		}

		/**
		 * Decrements the value of the peer.
		 * @exceptionsafety Strong
		 * @threadsafety    Safe
		 */
		void decValue()
		{
			if (value > VALUE_MIN)
				--value;
		}

		/**
		 * Returns the value of the peer.
		 * @return The value of the peer
		 */
		PeerValue getValue() const
		{
			return value;
		}

		/**
		 * Resets the value of a peer.
		 * @exceptionsafety Nothrow
		 * @threadsafety    Compatible but not safe
		 */
		void resetValue()
		{
			value = 0;
		}

		/**
		 * Adds a send-action to the send-action queue.
		 * @param[in] action         Send-action to be added
		 * @param[in] maxResideTime  Maximum residence time in the queue for
		 *                           send-actions
		 * @return
		 */
		bool push(
				std::shared_ptr<SendAction> action,
				const TimeRes&              maxResideTime)
		{
			return sendQ.push(action, maxResideTime);
		}
	};

    /// An element in the set of active peers
	typedef std::shared_ptr<PeerWrapper> PeerEntry;

    std::unordered_map<Peer, PeerEntry> peerEntries;
    mutable std::mutex                  mutex;
    mutable std::condition_variable     cond;
    Time                                whenEligible;
    const TimeRes                       eligibilityDuration;
    const TimeRes                       maxResideTime;
    std::function<void(Peer&)>          peerFailed;
    unsigned                            maxPeers;
    SyncQueue<Peer>                     failedPeerQ;
    std::exception_ptr                  exception;
    std::thread                         failedPeerThread;

    /**
     * Indicates if insufficient time has passed to determine the
     * worst-performing peer.
     * @return `true`  Iff it's too soon
     */
    inline bool tooSoon()
    {
        assert(isLocked(mutex));
        return Clock::now() < whenEligible;
    }

    /**
     * Indicates if the set is full.
     * @return `true`  Iff set is full
     */
    inline bool full() const
    {
        assert(isLocked(mutex));
        return peerEntries.size() >= maxPeers;
    }

    /**
     * Indicates if this instance contains a remote peer with a given Internet
     * socket address.
     * @param[in] addr  Internet socket address
     * @return `true`   Iff this instance contains a remote peer with the given
     *                  Internet socket address
     */
    bool contains(const InetSockAddr& addr)
    {
        assert(isLocked(mutex));
        for (const auto& elt : peerEntries) {
            const Peer* peer = &elt.first;
            if (addr == peer->getRemoteAddr())
                return true;
        }
        return false;
    }

    /**
     * Unconditionally inserts a peer. The peer immediately starts receiving
     * messages from its associated remote peer and is ready to send messages.
     * If the inserted peer makes the set of peers full, then
     *   - All peer values are reset;
     *   - The eligibility timer is set.
     * @param[in] peer   Peer to be inserted
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    void insert(Peer& peer)
    {
        assert(isLocked(mutex));
        if (peerEntries.find(peer) == peerEntries.end()) {
            PeerEntry entry(new PeerWrapper(peer,
                    [this](Peer& p){handleFailedPeer(p);}));
            peerEntries.emplace(peer, entry);
            if (full()) {
                resetValues();
                setWhenEligible();
            }
        }
    }

    /**
     * Sets the time-point when the worst performing peer may be removed from
     * the set of peers.
     */
    void setWhenEligible()
    {
        assert(isLocked(mutex));
        whenEligible = Clock::now() + eligibilityDuration;
    }

    /**
     * Unconditionally removes a peer with the lowest value from the set of peers.
     * @return           The removed peer
     * @exceptionsafety  Nothrow
     * @threadsafety     Compatible but not safe
     */
    Peer removeWorstPeer()
    {
        assert(isLocked(mutex));
        assert(peerEntries.size() > 0);
        Peer worstPeer{};
        auto minValue = PeerWrapper::VALUE_MAX;
        for (const auto& elt : peerEntries) {
            auto value = elt.second->getValue();
            if (value < minValue) {
                minValue = value;
                worstPeer = elt.first;
            }
        }
        peerEntries.erase(worstPeer);
        return worstPeer;
    }

    /**
     * Resets the value-counts in the map from peer to value.
     * @exceptionsafety  Nothrow
     * @threadsafety     Compatible but not safe
     */
    void resetValues() noexcept
    {
        assert(isLocked(mutex));
        for (const auto& elt : peerEntries)
            elt.second->resetValue();
    }

    /**
     * Handles a failed peer. Called on one of the `PeerEntryImpl` threads.
     * @param[in] peer  The peer that failed
     */
    void handleFailedPeer(Peer& peer)
    {
		failedPeerQ.push(peer);
    }

    /**
     * Handles failed peers in the set of active peers. Removes a failed peer
     * from the set and notifies the failed-peer observer. Intended to run
     * on its own cancellable thread.
     */
    void handleFailedPeers()
    {
    	try {
    		for (;;) {
    			::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
    			auto peer = failedPeerQ.pop();
    			::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
				{
					std::lock_guard<decltype(mutex)> lock{mutex};
					peerEntries.erase(peer);
					cond.notify_one();
				}
				peerFailed(peer);
    		}
    	}
    	catch (const std::exception& e) {
    		exception = std::current_exception();
    	}
    }

public:
    /**
     * Constructs from the maximum number of peers. The set will be empty.
     * @param[in] peerFailed          Function to call when a peer fails
     * @param[in] maxPeers            Maximum number of peers
     * @param[in] stasisDuration      Required duration, in seconds, without
     *                                change to the set of peers before the
     *                                worst-performing peer may be replaced
     * @throws std::invalid_argument  `maxPeers == 0`
     */
    Impl(   std::function<void(Peer&)> peerFailed,
            const unsigned             maxPeers,
            const unsigned             stasisDuration)
        : peerEntries{}
        , mutex{}
        , cond{}
        , whenEligible{}
        , eligibilityDuration{std::chrono::seconds{stasisDuration}}
        , maxResideTime{eligibilityDuration*2}
        , peerFailed{peerFailed}
        , maxPeers{maxPeers}
        , failedPeerQ{}
        , exception{}
        , failedPeerThread{}
    {
        if (maxPeers == 0)
            throw InvalidArgument(__FILE__, __LINE__,
            		"Maximum number of peers can't be zero");
        failedPeerThread = std::thread([this]{handleFailedPeers();});
    }

    /// Destroys
    ~Impl()
    {
    	{
			std::unique_lock<decltype(mutex)> lock{mutex};
			peerEntries.clear();
    	}
    	if (failedPeerThread.joinable()) {
    		::pthread_cancel(failedPeerThread.native_handle());
    		failedPeerThread.join();
    	}
    }

    /// Prevents copy and move construction and assignment
    Impl(const Impl& that) =delete;
    Impl(const Impl&& that) =delete;
    Impl& operator =(const Impl& rhs) =delete;
    Impl& operator =(const Impl&& rhs) =delete;

    /**
     * Tries to insert a peer.
     * @param[in]  candidate  Candidate peer
     * @param[out] size       Number of active peers
     * @return                Insertion status:
     *   - EXISTS    Peer is already member of set
     *   - SUCCESS   Success
     *   - FULL      Set is full and insufficient time to determine worst peer
     * @exceptionsafety       Strong guarantee
     * @threadsafety          Safe
     */
    PeerSet::InsertStatus tryInsert(
            Peer&     candidate,
            size_t*   size)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
    	if (exception)
    		std::rethrow_exception(exception);
        InsertStatus                     status;
        if (peerEntries.find(candidate) != peerEntries.end()) {
            status = PeerSet::EXISTS; // Candidate peer is already a member
        }
        else if (!full()) {
            // Just add the candidate peer
            insert(candidate);
            status = PeerSet::SUCCESS;
            cond.notify_one();
        }
        else if (tooSoon()) {
            status = PeerSet::FULL;
        }
        else {
            // Replace worst-performing peer
            Peer worst{removeWorstPeer()};
            insert(candidate);
            status = PeerSet::SUCCESS;
        }
        if (size)
            *size = peerEntries.size();
        return status;
    }

    /**
     * Sends information about a product to the remote peers.
     * @param[in] info            Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void sendNotice(const ProdInfo& info)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
    	if (exception)
    		std::rethrow_exception(exception);
        std::shared_ptr<SendProdNotice> action{new SendProdNotice(info)};
        for (const auto& elt : peerEntries)
            elt.second->push(action, maxResideTime);
    }

    /**
     * Sends information about a product to the remote peers except for one.
     * @param[in] info            Product information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void sendNotice(const ProdInfo& info, const Peer& except)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
    	if (exception)
    		std::rethrow_exception(exception);
        std::shared_ptr<SendProdNotice> action{new SendProdNotice(info)};
        for (const auto& elt : peerEntries) {
            if (elt.first == except)
                continue;
            elt.second->push(action, maxResideTime);
        }
    }

    /**
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] info            Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void sendNotice(const ChunkInfo& info)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
    	if (exception)
    		std::rethrow_exception(exception);
        std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(info)};
        for (const auto& elt : peerEntries)
            elt.second->push(action, maxResideTime);
    }

    /**
     * Sends information about a chunk-of-data to the remote peers except for
     * one.
     * @param[in] info            Chunk information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void sendNotice(const ChunkInfo& info, const Peer except)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
    	if (exception)
    		std::rethrow_exception(exception);
        std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(info)};
        for (const auto& elt : peerEntries) {
            if (elt.first == except)
                continue;
            elt.second->push(action, maxResideTime);
        }
    }

    /**
     * Increments the value of a peer. Does nothing if the peer isn't found.
     * @param[in] peer  Peer to have its value incremented
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void incValue(Peer& peer)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
    	if (exception)
    		std::rethrow_exception(exception);
        if (full()) {
            auto iter = peerEntries.find(peer);
            if (iter != peerEntries.end())
                iter->second->incValue();
        }
    }

    /**
     * Decrements the value of a peer. Does nothing if the peer isn't found.
     * @param[in] peer  Peer to have its value decremented
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void decValue(Peer& peer)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
    	if (exception)
    		std::rethrow_exception(exception);
        if (full()) {
            auto iter = peerEntries.find(peer);
            if (iter != peerEntries.end())
                iter->second->decValue();
        }
    }

    /**
     * Returns the number of peers in the set.
     * @return Number of peers in the set
     */
    size_t size() const
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
    	if (exception)
    		std::rethrow_exception(exception);
        return peerEntries.size();
    }

    /**
     * Indicates if this instance is full.
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    bool isFull() const
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
    	if (exception)
    		std::rethrow_exception(exception);
        return full();
    }
};

PeerSet::PeerSet(
        std::function<void(Peer&)> peerTerminated,
        const unsigned             maxPeers,
        const unsigned             stasisDuration)
    : pImpl(new Impl(peerTerminated, maxPeers, stasisDuration))
{}

PeerSet::InsertStatus PeerSet::tryInsert(
        Peer&   candidate,
        size_t* size) const
{
    return pImpl->tryInsert(candidate, size);
}

void PeerSet::sendNotice(const ProdInfo& prodInfo)
{
    pImpl->sendNotice(prodInfo);
}

void PeerSet::sendNotice(const ProdInfo& prodInfo, const Peer& except)
{
    pImpl->sendNotice(prodInfo, except);
}

void PeerSet::sendNotice(const ChunkInfo& chunkInfo)
{
    pImpl->sendNotice(chunkInfo);
}

void PeerSet::sendNotice(const ChunkInfo& chunkInfo, const Peer& except)
{
    pImpl->sendNotice(chunkInfo, except);
}

void PeerSet::incValue(Peer& peer) const
{
    pImpl->incValue(peer);
}

void PeerSet::decValue(Peer& peer) const
{
    pImpl->decValue(peer);
}

size_t PeerSet::size() const
{
    return pImpl->size();
}

bool PeerSet::isFull() const
{
    return pImpl->isFull();
}

} // namespace
