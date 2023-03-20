/**
 * Thread-safe list of addresses of peer servers.
 *
 *        File: Tracker.h
 *  Created on: Jun 29, 2019
 *      Author: Steven R. Emmerson
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

#ifndef MAIN_P2P_TRACKER_H_
#define MAIN_P2P_TRACKER_H_

#include "SockAddr.h"
#include "Xprt.h"

#include <memory>

namespace hycast {

/**
 * Tracks available P2P servers.
 */
class Tracker final : public XprtAble
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs. The list will be empty.
     *
     * @param[in] capacity  Capacity in socket addresses.
     */
    explicit Tracker(const size_t capacity = 100);

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    std::string to_string() const;

    /**
     * Returns the number of entries.
     * @return The number of entries
     */
    size_t size() const;

    /**
     * Inserts the address of a peer server if it doesn't already exist.
     * If the capacity is exceeded, then the oldest entries are deleted.
     *
     * @param[in] peerSrvrAddr       Socket address of peer server
     * @retval    true               Success
     * @retval    false              Address already exists
     * @exceptionsafety              Strong guarantee
     * @threadsafety                 Safe
     */
    bool insert(const SockAddr& peerSrvrAddr) const;

    /**
     * Inserts the entries from another instance.
     * @param[in] tracker  The other instance
     */
    void insert(const Tracker tracker) const;

    /**
     * Removes a P2P server.
     * @param p2pSrvrAddr  Socket address of the P2P server
     */
    void erase(const SockAddr p2pSrvrAddr);

    /**
     * Removes the P2P server addresses contained in another instance.
     * @param tracker  The other instance
     */
    void erase(const Tracker tracker);

    /**
     * Removes and returns the first P2P server address.
     * @return
     */
    SockAddr removeHead() const;

    /**
     * Causes `removeHead()` to always return a socket address that tests false. Idempotent.
     */
    void halt() const;

    /**
     * Writes itself to a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(Xprt xprt) const;

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool read(Xprt xprt);
};

} // namespace

#endif /* MAIN_P2P_TRACKER_H_ */
