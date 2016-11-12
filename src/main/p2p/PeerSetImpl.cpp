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

#include <cstdint>
#include <stdexcept>

namespace hycast {

PeerSetImpl::PeerSetImpl(unsigned maxPeers)
    : maxPeers{maxPeers},
      set{},
      values{4*maxPeers/3},
      mutex{}
{
    if (maxPeers == 0)
        throw std::invalid_argument("Maximum number of peers is zero");
}

bool PeerSetImpl::insert(Peer& peer)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    if (set.size() >= maxPeers)
        return false;
    auto result = set.insert(peer);
    if (result.second)
        values[peer] = ValueEntry();
    return result.second;
}

void PeerSetImpl::sendNotice(const ProdInfo& prodInfo)
{
    for (const auto& peer : set)
        peer.sendNotice(prodInfo);
}

void PeerSetImpl::sendNotice(const ChunkInfo& chunkInfo)
{
    for (const auto& peer : set)
        peer.sendNotice(chunkInfo);
}

void PeerSetImpl::incValue(const Peer& peer)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    auto valueEntryPtr = values.find(peer);
    if (valueEntryPtr != values.end() &&
            valueEntryPtr->second.value < ValueEntry::VALUE_MAX)
        ++valueEntryPtr->second.value;
}

std::pair<bool, Peer> PeerSetImpl::removeWorstEligible()
{
    std::pair<bool, Peer> result;
    auto minValue = ValueEntry::VALUE_MAX + 1;
    for (auto elt : values) {
        if (elt.second.isEligible && elt.second.value < minValue) {
            minValue = elt.second.value;
            result.second = elt.first;
        }
    }
    result.first = minValue <= ValueEntry::VALUE_MAX;
    if (result.first) {
        values.erase(result.second);
        set.erase(result.second);
    }
    return result;
}

void PeerSetImpl::resetValues()
{
    for (auto elt : values) {
        elt.second.value = 0;
        elt.second.isEligible = true;
    }
}

std::pair<bool, Peer> PeerSetImpl::possiblyRemoveWorst()
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    std::pair<bool, Peer> result;
    result.first = false;
    if (set.size() < maxPeers) {
        result.first = false;
    }
    else {
        result = removeWorstEligible();
    }
    resetValues(); // Always done to make just-added peers eligible for removal
    return result;
}

} // namespace
