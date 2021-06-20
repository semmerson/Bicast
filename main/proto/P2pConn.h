/**
 * This file declares classes related to a Hycast protocol connection.
 *
 *  @file:  P2pConn.h
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

#ifndef MAIN_PROTO_P2PCONN_H_
#define MAIN_PROTO_P2PCONN_H_

#include "HycastProto.h"
#include "SockAddr.h"
#include "Socket.h"

#include <memory>

namespace hycast {

/**
 * Hycast connection with a remote peer.
 */
class P2pConn
{
    friend class P2pSrvr;

public:
    class                 Impl;

private:
    std::shared_ptr<Impl> pImpl;

    /**
     * Sets the next, individual connection. Server-side only.
     *
     * @param[in] sock        Relevant socket
     * @retval    `false`     P2P connection isn't complete
     * @retval    `true`      P2P connection is complete
     * @throw     LogicError  Connection is already complete
     */
    bool set(TcpSock sock);

public:
    /**
     * Default constructs.
     */
    P2pConn();

    /**
     * Client-side construction.
     *
     * @param[in] srvrAddr  Socket address of Hycast server
     */
    explicit P2pConn(SockAddr srvrAddr);

    operator bool() noexcept;

    /**
     * Notifies the remote peer.
     */
    void notify(const PubPath notice);
    void notify(const ProdIndex notice);
    void notify(const DataSegId& notice);

    /**
     * Requests data from the remote peer.
     */
    void request(const ProdIndex request);
    void request(const DataSegId& request);

    /**
     * Sends data to the remote peer.
     */
    void send(const ProdInfo& prodInfo);
    void send(const DataSeg& dataSeg);
};

/**
 * Listening P2P server.
 */
class P2pSrvr
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs from the local address for the server.
     *
     * @param[in] srvrAddr   Local Address for P2P server
     * @param[in] maxAccept  Maximum number of outstanding P2P connections
     */
    P2pSrvr(const SockAddr& srvrAddr,
            const unsigned  maxAccept = 8);

    /**
     * Returns the next, accepted, peer-to-peer connection.
     *
     * @return Next P2P connection
     */
    P2pConn accept();
};

} // namespace

#endif /* MAIN_PROTO_P2PCONN_H_ */
