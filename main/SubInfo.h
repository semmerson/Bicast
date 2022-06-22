/**
 * This file declares stuff common to a Hycast publisher and Hycast subscriber.
 *
 *  @file:  PubSub.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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

#ifndef MAIN_PUBSUB_H_
#define MAIN_PUBSUB_H_

#include "HycastProto.h"
#include "Xprt.h"

namespace hycast {

// Subscription information for a subscriber
struct SubInfo : public XprtAble {
    uint16_t  version;     ///< Protocol version
    String    feedName;    ///< Name of data-product stream
    SegSize   maxSegSize;  ///< Maximum size of a data-segment in bytes
    struct Mcast {
        SockAddr dstAddr;  ///< Multicast destination address
        InetAddr srcAddr;  ///< Multicast source address
        Mcast()
            : dstAddr()
            , srcAddr()
        {}
    }         mcast;
    Tracker   tracker;     ///< Pool of P2P server addresses
    uint32_t  keepTime;    ///< Duration to keep data-products in seconds
    SubInfo(Tracker tracker)
        : version(1)
        , feedName("Hycast")
        , maxSegSize(1444)
        , mcast()
        , tracker(tracker)
        , keepTime(3600)
    {}
    SubInfo(const unsigned trackerSize)
        : SubInfo(Tracker{trackerSize})
    {}
    SubInfo()
        : SubInfo(100)
    {}
    bool write(Xprt xprt) const {
        return
                xprt.write(version) &&
                xprt.write<uint8_t>(feedName) &&
                xprt.write(maxSegSize) &&
                mcast.dstAddr.write(xprt) &&
                mcast.srcAddr.write(xprt) &&
                tracker.write(xprt) &&
                xprt.write(keepTime);
    }
    bool read(Xprt xprt) {
        auto success =
                xprt.read(version) &&
                xprt.read<uint8_t>(feedName) &&
                xprt.read(maxSegSize) &&
                mcast.dstAddr.read(xprt) &&
                mcast.srcAddr.read(xprt) &&
                tracker.read(xprt) &&
                xprt.read(keepTime);
        if (success && mcast.dstAddr.getInetAddr().getFamily() != mcast.srcAddr.getFamily())
            throw LOGIC_ERROR("Family of multicast address " + mcast.dstAddr.to_string() +
                    " != family of source address " + mcast.srcAddr.to_string());
        return success;
    }
};

} // namespace

#endif /* MAIN_PUBSUB_H_ */
