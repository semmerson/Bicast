/**
 * This file declares subscription information common to a Bicast publisher and Bicast subscriber.
 *
 *  @file:  SubInfo.h
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

#include "BicastProto.h"
#include "logging.h"
#include "Tracker.h"
#include "Xprt.h"

namespace bicast {

/// Subscription information for a subscriber
struct SubInfo : public XprtAble {
    uint16_t  version;     ///< Protocol version
    String    feedName;    ///< Name of data-product stream
    SegSize   maxSegSize;  ///< Maximum size of a data-segment in bytes
    /// Information on the source-specific multicast
    struct Mcast {
        SockAddr dstAddr;  ///< Multicast destination address
        InetAddr srcAddr;  ///< Multicast source address
    }            mcast;    ///< Multicast parameters
    Tracker      tracker;  ///< Information on potential P2P-servers
    SysDuration  keepTime; ///< Duration to keep data-products

     /**
      * Constructs from a tracker.
      * @param[in] tracker  Pool of potential P2P servers
      */
    SubInfo(Tracker tracker)
        : version(1)
        , feedName("Bicast")
        , maxSegSize(1444)
        , mcast()
        , tracker(tracker)
        , keepTime(std::chrono::hours(1))
    {}

    /**
     * Constructs from the size of the tracker.
     * @param[in] trackerSize  Maximum capacity of the subscriber's P2P server tracker
     */
    SubInfo(const unsigned trackerSize)
        : SubInfo(Tracker{trackerSize})
    {}

    /**
     * Default constructs.
     */
    SubInfo()
        : SubInfo(100)
    {}

    /**
     * Returns a string representation.
     * @return A string representation
     */
    String to_string() const {
        return "{vers=" + std::to_string(version) + ", feed=" + feedName + ", segSize=" +
                std::to_string(maxSegSize) + ", mcast={dst=" + mcast.dstAddr.to_string() +
                ", src=" + mcast.srcAddr.to_string() + "}, trackerSize=" +
                std::to_string(tracker.size()) + ", keepTime=" + std::to_string(keepTime) + "}";
    }

    /**
     * Writes itself to a transport.
     * @param[in] xprt  The transport
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(Xprt& xprt) const {
#if 0
        return
                xprt.write(version) &&
                xprt.write<uint8_t>(feedName) &&
                xprt.write(maxSegSize) &&
                mcast.dstAddr.write(xprt) &&
                mcast.srcAddr.write(xprt) &&
                tracker.write(xprt) &&
                xprt.write(keepTime);
#else
        LOG_TRACE("Writing version");
        auto success = xprt.write(version);
        if (success) {
            LOG_TRACE("Writing feedName");
            success = success && xprt.write<uint8_t>(feedName);
        }
        if (success) {
            LOG_TRACE("Writing maxSegSize");
            success = success && xprt.write(maxSegSize);
        }
        if (success) {
            LOG_TRACE("Writing dstAddr");
            success = success && mcast.dstAddr.write(xprt);
        }
        if (success) {
            LOG_TRACE("Writing srcAddr");
            success = success && mcast.srcAddr.write(xprt);
        }
        if (success) {
            LOG_TRACE("Writing tracker");
            success = success && tracker.write(xprt);
        }
        if (success) {
            LOG_TRACE("Writing keepTime");
            success = success && xprt.write(keepTime);
        }
        return success;
#endif
    }
    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool read(Xprt& xprt) {
#if 0
        auto success =
                xprt.read(version) &&
                xprt.read<uint8_t>(feedName) &&
                xprt.read(maxSegSize) &&
                mcast.dstAddr.read(xprt) &&
                mcast.srcAddr.read(xprt) &&
                tracker.read(xprt) &&
                xprt.read(keepTime);
#else
        LOG_TRACE("Reading version");
        bool success = xprt.read(version);
        if (success) {
            LOG_TRACE("Reading feedName");
            success = success && xprt.read<uint8_t>(feedName);
        }
        if (success) {
            LOG_TRACE("Reading maxSegSize");
            success = success && xprt.read(maxSegSize);
        }
        if (success) {
            LOG_TRACE("Reading dstAddr");
            success = success && mcast.dstAddr.read(xprt);
        }
        if (success) {
            LOG_TRACE("Reading srcAddr");
            success = success && mcast.srcAddr.read(xprt);
        }
        if (success) {
            LOG_TRACE("Reading tracker");
            success = success && tracker.read(xprt);
        }
        if (success) {
            LOG_TRACE("Reading keepTime");
            success = success && xprt.read(keepTime);
        }
#endif
        if (success && mcast.dstAddr.getInetAddr().getFamily() != mcast.srcAddr.getFamily())
            throw LOGIC_ERROR("Family of multicast address " + mcast.dstAddr.to_string() +
                    " != family of source address " + mcast.srcAddr.to_string());
        return success;
    }
};

} // namespace

#endif /* MAIN_PUBSUB_H_ */
