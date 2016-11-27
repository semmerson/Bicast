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
#include <stdexcept>

namespace hycast {

bool isLocked(std::recursive_mutex& mutex)
{
    if (!mutex.try_lock())
        return false;
    mutex.unlock();
    return true;
}

bool PeerSetImpl::PeerActionQ::push(
        std::shared_ptr<PeerAction> action,
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

void PeerSetImpl::PeerActionQ::sendNotice(const ProdInfo& prodInfo,
        const TimeRes& maxResideTime)
{
    std::shared_ptr<SendProdNotice> action{new SendProdNotice(prodInfo)};
    push(action, maxResideTime);
}

void PeerSetImpl::PeerActionQ::sendNotice(const ChunkInfo& chunkInfo,
        const TimeRes& maxResideTime)
{
    std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(chunkInfo)};
    push(action, maxResideTime);
}

std::shared_ptr<PeerSetImpl::PeerAction> PeerSetImpl::PeerActionQ::pop()
{
    std::unique_lock<decltype(mutex)> lock{mutex};
    while (queue.empty())
        cond.wait(lock);
    auto action = queue.front();
    queue.pop();
    cond.notify_one();
    return action;
}

void PeerSetImpl::PeerActionQ::terminate()
{
    std::unique_lock<decltype(mutex)> lock{mutex};
    while (!queue.empty())
        queue.pop();
    queue.push(std::shared_ptr<PeerAction>(new TerminatePeer()));
    cond.notify_one();
}

PeerSetImpl::PeerSetImpl(
        const unsigned maxPeers,
        const TimeRes  minDuration)
    : maxPeers{maxPeers},
      actionQs{},
      values{4*maxPeers/3},
      mutex{},
      incValueEnabled{false},
      whenEligible{},
      eligibilityDuration{minDuration},
      maxResideTime{minDuration*2}
{
    if (maxPeers == 0)
        throw std::invalid_argument("Maximum number of peers can't be zero");
}

void PeerSetImpl::remove(const Peer& peer)
{
    assert(isLocked(mutex));
    actionQs.erase(peer);
    values.erase(peer);
    // TODO: Call something to try adding another peer
}

void PeerSetImpl::terminate(const Peer& peer)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    auto iter = actionQs.find(peer);
    if (iter != actionQs.end())
        iter->second.terminate();
    remove(peer);
}

void PeerSetImpl::processPeerActions(
        const Peer&  peer,
        PeerActionQ& actionQ)
{
    try {
        for (;;) {
            std::shared_ptr<PeerAction> action{actionQ.pop()};
            action->actUpon(peer);
            if (action->terminate())
                break;
        }
    }
    catch (const std::exception& e) {
        log_what(e); // Because end of thread
    }
    terminate(peer);
}

void PeerSetImpl::runReceiver(const Peer& peer)
{
    try {
        peer.runReceiver();
    }
    catch (const std::exception& e) {
        log_what(e); // Because end of thread
    }
    terminate(peer);
}

void PeerSetImpl::insert(const Peer& peer)
{
    assert(isLocked(mutex));
    values[peer] = 0;
    try {
        auto iter = actionQs.emplace(peer, PeerActionQ()).first;
        std::thread([&] { processPeerActions(iter->first, iter->second); })
                .detach();
        std::thread([&] { runReceiver(peer); }).detach();
    }
    catch (const std::exception& e) {
        terminate(peer);
        throw;
    }
}

Peer PeerSetImpl::removeWorstPeer()
{
    assert(isLocked(mutex));
    Peer worstPeer{};
    auto minValue = VALUE_MAX;
    for (const auto& elt : values) {
        if (elt.second < minValue) {
            minValue = elt.second;
            worstPeer = elt.first;
        }
    }
    terminate(worstPeer);
    return worstPeer;
}

void PeerSetImpl::resetValues()
{
    assert(isLocked(mutex));
    for (auto& elt : values)
        elt.second = 0;
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
    if (actionQs.find(candidate) != actionQs.end())
        return FAILURE; // Candidate peer is already a member
    if (actionQs.size() < maxPeers) {
        // Set not full. Just add the candidate peer
        insert(candidate);
        if (actionQs.size() == maxPeers) {
            incValueEnabled = true;
            setWhenEligible();
        }
        return SUCCESS;
    }
    if (Clock::now() < whenEligible)
        return FAILURE; // Insufficient duration to determine worst peer
    *replaced = removeWorstPeer();
    insert(candidate);
    resetValues();
    setWhenEligible();
    return REPLACED;
}

void PeerSetImpl::sendNotice(const ProdInfo& info)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    for (auto& peerEntry : actionQs)
        peerEntry.second.sendNotice(info, maxResideTime);
}

void PeerSetImpl::sendNotice(const ChunkInfo& info)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    for (auto& peerEntry : actionQs)
        peerEntry.second.sendNotice(info, maxResideTime);
}

void PeerSetImpl::incValue(const Peer& peer)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    if (incValueEnabled) {
        auto iter = values.find(peer);
        if (iter != values.end() && iter->second < VALUE_MAX)
            ++iter->second;
    }
}

} // namespace
