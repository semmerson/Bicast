/**
 * This file declares the interface for a peer-to-peer node. Such a node
 * is called by peers to handle received PDU-s.
 * 
 * @file:   P2pNode.h
 * @author: Steven R. Emmerson
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

#ifndef MAIN_PROTO_P2PNODE_H_
#define MAIN_PROTO_P2PNODE_H_

#include "error.h"
#include "HycastProto.h"

#include <cstdint>
#include <string>

namespace hycast {

class Peer; // Forward declaration

/// Interface
class P2pNode : public RequestRcvr
              , public NoticeRcvr
              , public DataRcvr
{
public:
    virtual ~P2pNode() {}

    virtual bool isPublisher() const {
        return false;
    }

    virtual bool isPathToPub() const {
        return false;
    }

    virtual void recvNotice(const PubPath    notice,
                            const SockAddr   rmtPeerAddr) =0;
    /**
     * Receives a notice of available product information from a remote peer.
     *
     * @param[in] notice       Which product
     * @param[in] rmtPeerAddr  Socket address of remote peer
     * @retval    `false`      Local peer shouldn't request from remote peer
     * @retval    `true`       Local peer should request from remote peer
     */
    virtual bool recvNotice(const ProdIndex  notice,
                            const SockAddr   rmtPeerAddr) =0;
    /**
     * Receives a notice of an available data-segment from a remote peer.
     *
     * @param[in] notice       Which data-segment
     * @param[in] rmtPeerAddr  Socket address of remote peer
     * @retval    `false`      Local peer shouldn't request from remote peer
     * @retval    `true`       Local peer should request from remote peer
     */
    virtual bool recvNotice(const DataSegId notice,
                            const SockAddr  rmtPeerAddr) =0;

    /**
     * Receives a request for product information from a remote peer.
     *
     * @param[in] request      Which product
     * @param[in] rmtPeerAddr  Socket address of remote peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual ProdInfo recvRequest(const ProdIndex  request,
                                 const SockAddr   rmtPeerAddr) =0;
    /**
     * Receives a request for a data-segment from a remote peer.
     *
     * @param[in] request      Which data-segment
     * @param[in] rmtPeerAddr  Socket address of remote peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual DataSeg  recvRequest(const DataSegId request,
                                 const SockAddr  rmtPeerAddr) =0;

    virtual void recvData(const ProdInfo prodInfo,
                          const SockAddr rmtPeerAddr) =0;
    virtual void recvData(const DataSeg  dataSeg,
                          const SockAddr rmtPeerAddr) =0;
};

} // namespace

#endif /* MAIN_PROTO_P2PNODE_H_ */
