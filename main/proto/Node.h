/**
 * This file declares the interface Node. A Node interacts with a Peer to send
 * and receive data-products.
 * 
 * @file:   Node.h
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

#ifndef MAIN_PROTO_NODE_H_
#define MAIN_PROTO_NODE_H_

#include "HycastProto.h"
#include "Peer.h"

#include <cstdint>
#include <string>

namespace hycast {

/// Publishing/sending node
class PubNode : public RequestRcvr
{
public:
    virtual ~PubNode();

    virtual void recvRequest(const ProdIndex& request,
                             Peer&            peer) =0;
    virtual void recvRequest(const DataSegId& request,
                             Peer&            peer) =0;
};

/// Subscribing/receiving node
class SubNode : public McastRcvr,
                public RequestRcvr,
                public NoticeRcvr,
                public DataRcvr
{
public:
    virtual ~SubNode();

    virtual bool isPathToPub();

    virtual void recvMcast(const ProdInfo& prodInfo) =0;
    virtual void recvMcast(const DataSeg& dataSeg) =0;

    virtual void recvRequest(const ProdIndex& request,
                             Peer&            peer) =0;
    virtual void recvRequest(const DataSegId& request,
                             Peer&            peer) =0;

    virtual void recvNotice(const ProdIndex& notice,
                            Peer&            peer) =0;
    virtual void recvNotice(const DataSegId&  notice,
                            Peer&             peer) =0;
    virtual void recvNotice(const PubPathNotice&  notice,
                            Peer&                 peer) =0;

    virtual void recvData(const ProdInfo& prodInfo,
                          Peer&           peer) =0;
    virtual void recvData(const DataSeg&  dataSeg,
                          Peer&           peer) =0;
};

} // namespace

#endif /* MAIN_PROTO_NODE_H_ */
/**
 * This file declares the interface Node. A Node interacts with a Peer to send
 * and receive data-products.
 * 
 * @file:   Node.h
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

#ifndef MAIN_PROTO_NODE_H_
#define MAIN_PROTO_NODE_H_

#include "error.h"
#include "HycastProto.h"
#include "Peer.h"

#include <cstdint>
#include <string>

namespace hycast {

/// Interface
class Node : public RequestRcvr,
             public NoticeRcvr,
             public DataRcvr
{
public:
    virtual ~Node() {}

    virtual bool isPublisher() const =0;

    virtual bool isPathToPub() const =0;

    virtual void recvRequest(const ProdIndex& request,
                             Peer&            peer) =0;
    virtual void recvRequest(const DataSegId& request,
                             Peer&            peer) =0;

    virtual void recvNotice(const ProdIndex&      notice,
                            Peer&                 peer) =0;
    virtual void recvNotice(const DataSegId&      notice,
                            Peer&                 peer) =0;
    virtual void recvNotice(const PubPathNotice&  notice,
                            Peer&                 peer) =0;

    virtual void recvData(const ProdInfo& prodInfo,
                          Peer&           peer) =0;
    virtual void recvData(const DataSeg&  dataSeg,
                          Peer&           peer) =0;
};

} // namespace

#endif /* MAIN_PROTO_NODE_H_ */
