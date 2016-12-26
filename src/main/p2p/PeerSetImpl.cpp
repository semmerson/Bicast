/**
 * This file defines the implementation of a set of peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSetImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "PeerSetImpl.h"
#include "logging.h"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <exception>
#include <pthread.h>
#include <stdexcept>

namespace hycast {

bool isLocked(std::mutex& mutex)
{
    if (!mutex.try_lock())
        return true;
    mutex.unlock();
    return false;
}

bool PeerSetImpl::SendQ::push(
        std::shared_ptr<SendAction> action,
        const TimeRes&              maxResideTime)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    if (!queue.empty()) {
        if (maxResideTime <
                (action->getCreateTime() - queue.front()->getCreateTime()))
            return false;
    }
    queue.push(action);
    cond.notify_one();
    return true;
}

std::shared_ptr<PeerSetImpl::SendAction> PeerSetImpl::SendQ::pop()
{
    std::unique_lock<decltype(mutex)> lock{mutex};
    while (queue.empty())
        cond.wait(lock);
    auto action = queue.front();
    queue.pop();
    cond.notify_one();
    return action;
}

void PeerSetImpl::SendQ::terminate()
{
    std::unique_lock<decltype(mutex)> lock{mutex};
    while (!queue.empty())
        queue.pop();
    queue.push(std::shared_ptr<SendAction>(new TerminatePeer()));
    cond.notify_one();
}

PeerSetImpl::PeerEntryImpl::~PeerEntryImpl()
{
    // Destructors must never throw an exception
    try {
        sendQ.terminate();
        // Receiving code can handle cancellation
        ::pthread_cancel(recvThread.native_handle());
        sendThread.join();
        recvThread.join();
    }
    catch (const std::exception& e) {
        log_what(e);
    }
}

void PeerSetImpl::PeerEntryImpl::processSendQ()
{
    try {
        for (;;) {
            std::shared_ptr<SendAction> action{sendQ.pop()};
            action->actUpon(peer);
            if (action->terminate())
                break;
        }
    }
    catch (const std::exception& e) {
        log_what(e); // Because end of thread
        handleFailure(peer);
    }
}

void PeerSetImpl::PeerEntryImpl::runReceiver()
{
    try {
        peer.runReceiver();
    }
    catch (const std::exception& e) {
        log_what(e); // Because end of thread
        handleFailure(peer);
    }
}

PeerSetImpl::PeerSetImpl(
        const unsigned maxPeers,
        const unsigned stasisDuration)
    : peerEntries{}
    , mutex{}
    , whenEligible{}
    , eligibilityDuration{std::chrono::seconds{stasisDuration}}
    , maxResideTime{eligibilityDuration*2}
    , maxPeers{maxPeers}
    , incValueEnabled{false}
{
    if (maxPeers == 0)
        throw std::invalid_argument("Maximum number of peers can't be zero");
}

void PeerSetImpl::insert(Peer& peer)
{
    assert(isLocked(mutex));
    if (peerEntries.find(peer) == peerEntries.end()) {
        PeerEntry entry(new PeerEntryImpl(peer, [=](Peer& p){ handleFailure(p); }));
        peerEntries.emplace(peer, entry);
    }
}

Peer PeerSetImpl::removeWorstPeer()
{
    assert(isLocked(mutex));
    assert(peerEntries.size() > 0);
    Peer worstPeer{};
    auto minValue = PeerEntryImpl::VALUE_MAX;
    for (const auto& elt : peerEntries) {
        auto value = elt.second->getValue();
        if (value <= minValue) {
            minValue = value;
            worstPeer = elt.first;
        }
    }
    peerEntries.erase(worstPeer);
    return worstPeer;
}

void PeerSetImpl::resetValues()
{
    assert(isLocked(mutex));
    for (auto& elt : peerEntries)
        elt.second->resetValue();
}

void PeerSetImpl::setWhenEligible()
{
    assert(isLocked(mutex));
    whenEligible = Clock::now() + eligibilityDuration;
}

PeerSetImpl::InsertStatus PeerSetImpl::tryInsert(
        Peer& candidate,
        Peer* replaced)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    if (peerEntries.find(candidate) != peerEntries.end())
        return FAILURE; // Candidate peer is already a member
    if (peerEntries.size() < maxPeers) {
        // Set not full. Just add the candidate peer
        insert(candidate);
        if (peerEntries.size() == maxPeers) {
            incValueEnabled = true;
            setWhenEligible();
        }
        return SUCCESS;
    }
    if (Clock::now() < whenEligible)
        return FAILURE; // Insufficient duration to determine worst peer
    Peer worst{removeWorstPeer()};
    if (replaced)
        *replaced = worst;
    insert(candidate);
    resetValues();
    setWhenEligible();
    return REPLACED;
}

void PeerSetImpl::sendNotice(const ProdInfo& info)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    std::shared_ptr<SendProdNotice> action{new SendProdNotice(info)};
    for (auto& elt : peerEntries)
        elt.second->push(action, maxResideTime);
}

void PeerSetImpl::sendNotice(const ChunkInfo& info)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(info)};
    for (auto& elt : peerEntries)
        elt.second->push(action, maxResideTime);
}

void PeerSetImpl::incValue(Peer& peer)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    if (incValueEnabled) {
        auto iter = peerEntries.find(peer);
        if (iter != peerEntries.end())
            iter->second->incValue();
    }
}

void PeerSetImpl::handleFailure(Peer& peer)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    peerEntries.erase(peer);
}

} // namespace
