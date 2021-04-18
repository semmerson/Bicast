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
#include "Node.h"
#include "Socket.h"

#include <memory>

namespace hycast {

/// Handles low-level, bidirectional messaging with a remote peer.
class Peer final
{
public:
    class                 Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    Peer() =default;

    /**
     * Constructs a publishing instance from an `accept()`ed (i.e., server-side)
     * socket.
     */
    Peer(TcpSock& sock, PubNode& node);

    /**
     * Constructs a subscribing instance from an `accept()`ed (i.e.,
     * server-side) socket.
     */
    Peer(TcpSock& sock, SubNode& node);

    /**
     * Constructs a subscribing instance from a `connect()`ed (i.e.,
     * client-site) socket.
     */
    Peer(SockAddr& srvrAddr, SubNode& node);

    /**
     * Indicates if this instance is invalid (i.e., was default constructed).
     */
    operator bool() const {
        return static_cast<bool>(pImpl);
    }

    /**
     * Notifies the remote peer.
     */
    void notify(const PubPathNotice& notice) const;
    void notify(const ProdIndex& notice) const;
    void notify(const DataSegId& notice) const;

    /**
     * Requests data from the remote peer.
     */
    void request(const ProdIndex& request) const;
    void request(const DataSegId& request) const;

    /**
     * Sends data to the remote peer.
     */
    void send(const ProdInfo& prodInfo) const;
    void send(const DataSeg& dataSeg) const;
};

} // namespace

#endif /* MAIN_PROTO_PEER_H_ */
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
#include "Node.h"
#include "Socket.h"

#include <memory>

namespace hycast {

/// Handles low-level, bidirectional messaging with a remote peer.
class Peer final
{
public:
    class                 Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    Peer() =default;

    /**
     * Constructs an instance from an `accept()`ed (i.e., server-side)
     * socket.
     *
     * @param[in] sock  `accept()ed` socket
     * @param[in] node  Associated node
     */
    Peer(TcpSock& sock, Node& node);

    /**
     * Constructs an instance from the address of a server to which to
     * `connect()`.
     */
    Peer(SockAddr& srvrAddr, Node& node);

    void receive();

    /**
     * Indicates if this instance is invalid (i.e., was default constructed).
     */
    operator bool() const {
        return static_cast<bool>(pImpl);
    }

    /**
     * Notifies the remote peer.
     */
    void notify(const PubPathNotice& notice) const;
    void notify(const ProdIndex& notice) const;
    void notify(const DataSegId& notice) const;

    /**
     * Requests data from the remote peer.
     */
    void request(const ProdIndex& request) const;
    void request(const DataSegId& request) const;

    /**
     * Sends data to the remote peer.
     */
    void send(const ProdInfo& prodInfo) const;
    void send(const DataSeg& dataSeg) const;
};

} // namespace

#endif /* MAIN_PROTO_PEER_H_ */
