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
              , public P2pMgr
{
public:
    virtual ~P2pNode() {}

    virtual bool isPublisher() const {
        return false;
    }

    virtual bool isPathToPub() const {
        return false;
    }

    virtual void recvRequest(const ProdIndex  request,
                             Peer             peer) =0;
    virtual void recvRequest(const DataSegId& request,
                             Peer             peer) =0;

    virtual void recvNotice(const PubPath    notice,
                            Peer             peer) =0;
    virtual void recvNotice(const ProdIndex  notice,
                            Peer             peer) =0;
    virtual void recvNotice(const DataSegId& notice,
                            Peer             peer) =0;

    virtual void recvData(const ProdInfo& prodInfo,
                          Peer            peer) =0;
    virtual void recvData(const DataSeg&  dataSeg,
                          Peer            peer) =0;

    /**
     * Called when a peer's remote counterpart goes offline. Doesn't create a
     * new thread and returns immediately.
     *
     * @param[in] peer  Peer whose remote counterpart went offline
     */
    virtual void offline(Peer peer) =0;

    virtual void reassigned(const ProdIndex  notice,
                            Peer             peer) =0;
    virtual void reassigned(const DataSegId& notice,
                            Peer             peer) =0;
};

} // namespace

#endif /* MAIN_PROTO_P2PNODE_H_ */
