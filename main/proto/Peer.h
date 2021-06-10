/**
 * This file declares the Peer class. The Peer class handles low-level,
 * bidirectional messaging with its remote counterpart.
 *
 *  @file:  Peer.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
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

#ifndef MAIN_PROTO_PEER_H_
#define MAIN_PROTO_PEER_H_

#include "HycastProto.h"
#include "P2pNode.h"
#include "Socket.h"

#include <memory>

namespace hycast {

/// Handles low-level, bidirectional messaging with a remote peer.
class Peer final
{
private:
    class     Impl;
    using     SharedPtr = std::shared_ptr<Impl>;
    SharedPtr pImpl;

    Peer(SharedPtr& pImpl);

public:
    friend class Impl;

    /**
     * Default constructs.
     */
    Peer() =default;

    /**
     * Constructs a server-side instance.
     *
     * @param[in] sock  `accept()ed` socket
     * @param[in] node  Associated P2P node
     */
    Peer(TcpSock& sock, P2pNode& node);

    /**
     * Constructs a client-side instance.
     *
     * @param[in] srvrAddr  Address of server to which to connect
     * @param[in] node      P2P node to call about received PDU-s
     */
    Peer(const SockAddr& srvrAddr, P2pNode& node);

    /**
     * Starts this instance. Does the following:
     *   - If client-side constructed, blocks while connecting to the remote
     *     peer
     *   - Creates a thread on which
     *       - The connection is read; and
     *       - The P2P node is called.
     * Upon return, this instance must be stopped before being destroyed.
     *
     * @throw LogicError   Already started
     * @throw SystemError  Thread couldn't be created
     * @see   `stop()`
     */
    void start();

    /**
     * Stops this instance from serving its remote counterpart. Does the
     * following:
     *   - Cancels the thread on which the remote peer is being served
     *   - Joins with that thread
     *
     * @throw LogicError  Peer hasn't been started
     * @see   `start()`
     */
    void stop();

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval `false`  Instance is invalid
     * @retval `true`   Instance is valid
     */
    operator bool() const {
        return static_cast<bool>(pImpl);
    }

    size_t hash() const noexcept;

    bool operator<(const Peer rhs) const noexcept;

    bool operator==(const Peer& rhs) const noexcept;

    String to_string(bool withName = false) const;

    /**
     * Notifies the remote peer.
     */
    void notify(const PubPath notice) const;
    void notify(const ProdIndex notice) const;
    void notify(const DataSegId& notice) const;

    /**
     * Requests data from the remote peer.
     */
    void request(const ProdIndex request) const;
    void request(const DataSegId& request) const;

    /**
     * Sends data to the remote peer.
     */
    void send(const ProdInfo& prodInfo) const;
    void send(const DataSeg& dataSeg) const;

    bool rmtIsPubPath() const noexcept;
};

} // namespace

namespace std {
    template<>
    struct hash<hycast::Peer> {
        size_t operator()(const hycast::Peer& peer) const noexcept {
            return peer.hash();
        }
    };
}

#endif /* MAIN_PROTO_PEER_H_ */
