/**
 * @file: Tracker.h
 *
 * Thread-safe pool of addresses of peer servers.
 *
 * @file:   Tracker.h
 * @author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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

#include <memory>

namespace bicast {

/**
 * Tracks available P2P-servers.
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
     * @param[in] delay     Minimum delay before re-making available a the P2P-server associated with
     *                      a disconnected peer
     */
    explicit Tracker(
            const size_t       capacity = 1000,
            const SysDuration& delay = std::chrono::minutes(5));

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    std::string to_string() const;

    /**
     * Returns the capacity (i.e., the maximum number of entries).
     * @return The capacity
     */
    size_t getCapacity() const;

    /**
     * Returns the number of entries.
     * @return The number of entries
     */
    size_t size() const;

    /**
     * Tries to insert information on a P2P-server. If the server's address doesn't exist, then the
     * information is inserted; otherwise, the existing information is updated if the given
     * information is better. If the capacity is exceeded, then the worst entry is deleted.
     *
     * @param[in] srvrInfo  Information on a P2P-server
     * @retval    true      Success. New information inserted or updated.
     * @retval    false     More recent server information exists. No insertion.
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    bool insert(const P2pSrvrInfo& srvrInfo) const;

    /**
     * Inserts the entries from another instance.
     * @param[in] tracker  The other instance
     */
    void insert(const Tracker tracker) const;

    /**
     * Removes and returns the address of the next P2P-server to try. Blocks until one is available
     * or `halt()` has been called.
     * @return The address of the next P2P-server to try. Will test false if `halt()` has been
     *         called.
     * @see halt()
     */
    SockAddr getNextAddr() const;

    /**
     * Erases information on a P2P-server
     * @param[in] p2pSrvrAddr  Socket address of the P2P-server
     */
    void erase(const SockAddr p2pSrvrAddr) const;

    /**
     * Handles a peer disconnecting. If the local peer was constructed client-side, then the remote
     * P2P-server has its number of available server-side peers increased.
     * @param[in] p2pSrvrAddr  Socket address of the remote P2P-server
     * @param[in] wasClient    Was the local peer constructed client-side?
     * @retval true            P2P-server is known
     * @retval false           P2P-server is not known. Nothing was done.
     */
    bool disconnected(
            const SockAddr p2pSrvrAddr,
            const bool     wasClient) const;

    /**
     * Causes `getNextAddr()` to always return a socket address that tests false. Idempotent.
     */
    void halt() const;

    /**
     * Writes itself to a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(Xprt& xprt) const;

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool read(Xprt& xprt);
};

} // namespace

#endif /* MAIN_P2P_TRACKER_H_ */
