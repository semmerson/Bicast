/**
 * This file declares the types used in the Hycast protocol.
 * 
 * @file:   HycastProto.c
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

#ifndef FOO_BAR_H_
#define FOO_BAR_H_

#include <cstdint>
#include <string>
#include <time.h>

namespace hycast {

/******************************************************************************/
// Primitives

using ProdIndex = uint32_t;    ///< Index of data-product
using ProdSize  = uint32_t;    ///< Size of product in bytes
using SegSize   = uint16_t;    ///< Data-segment size in bytes
using SegOffset = ProdSize;    ///< Offset of data-segment in bytes
using String    = std::string; ///< Convenience

/// Data-segment identifier
struct DataSegId
{
    ProdIndex prodIndex; ///< Product index
    SegOffset offset;    ///< Offset of data segment in bytes

    DataSegId()
        : prodIndex{0}
        , offset{0}
    {}

    DataSegId(const ProdIndex prodIndex,
              const SegOffset offset)
        : prodIndex{prodIndex}
        , offset{offset}
    {}

    inline bool operator==(const DataSegId& rhs) const {
        return (prodIndex == rhs.prodIndex) && (offset == rhs.offset);
    }
};

/// Timestamp
struct Timestamp
{
    uint64_t sec;  ///< Seconds since the epoch
    uint32_t nsec; ///< Nanoseconds

    bool operator==(const Timestamp& rhs) const {
        return sec == rhs.sec && nsec == rhs.nsec;
    }
};

/******************************************************************************/
// Protocol data units (PDU)

/// Product information
struct ProdInfo
{
    ProdIndex index;   ///< Product index
    String    name;    ///< Name of product
    ProdSize  size;    ///< Size of product in bytes
    Timestamp created; ///< When product was created

public:
    ProdInfo() =default;

    ProdInfo(const ProdIndex    index,
             const std::string& name,
             const ProdSize     size)
        : index{index}
        , name(name)
        , size{size}
        , created{}
    {
        struct timespec now;
        ::clock_gettime(CLOCK_REALTIME, &now);
        created.sec = now.tv_sec;
        created.nsec = now.tv_nsec;
    }

    bool operator==(const ProdInfo& rhs) const {
        return index == rhs.index &&
               name == rhs.name &&
               size == rhs.size &&
               created == rhs.created;
    }
};

class Peer;

/// Data segment
struct DataSeg
{
    // Ethernet - IP header - TCP header - PduId - prodIndex - offset - prodSize
    static const SegSize CANON_DATASEG_SIZE = 1500 - 20 - 20 - 4 - 4 - 4 - 4;

    DataSegId   segId;    ///< Data-segment identifier
    /// Product size in bytes (for when product notice is missed)
    ProdSize    prodSize;
    const char* data;     ///< Data bytes

    static SegSize size(ProdSize prodSize, SegOffset offset) {
        SegSize nbytes = prodSize - offset;
        return (nbytes > CANON_DATASEG_SIZE)
                ? CANON_DATASEG_SIZE
                : nbytes;
    }

    DataSeg()
        : segId()
        , prodSize(0)
        , data(nullptr)
    {}

    DataSeg(const DataSegId& segId,
            const ProdSize   prodSize,
            const char*      data)
        : segId(segId)
        , prodSize{prodSize}
        , data{data}
    {}

    SegSize size() const {
        return size(prodSize, segId.offset);
    }
};

/// Path-to-publisher notice
using PubPathNotice = bool;

/******************************************************************************/
// Receiver/server interfaces:

/// Multicast receiver/server
class McastRcvr
{
public:
    virtual ~McastRcvr() {}
    virtual void recvMcast(const ProdInfo& prodInfo) =0;
    virtual void recvMcast(const DataSeg& dataSeg) =0;
};

class Peer;

/// Notice receiver/server
class NoticeRcvr
{
public:
    virtual ~NoticeRcvr() {}
    virtual void recvNotice(const PubPathNotice&  notice,
                            Peer&                 peer) =0;
    virtual void recvNotice(const ProdIndex& notice,
                            Peer&            peer) =0;
    virtual void recvNotice(const DataSegId&  notice,
                            Peer&             peer) =0;
};

/// Request receiver/server
class RequestRcvr
{
public:
    virtual ~RequestRcvr() {}
    virtual void recvRequest(const ProdIndex& request,
                             Peer&            peer) =0;
    virtual void recvRequest(const DataSegId& request,
                             Peer&            peer) =0;
};

/// Data receiver/server
class DataRcvr
{
public:
    virtual ~DataRcvr() {}
    virtual void recvData(const ProdInfo& prodInfo,
                          Peer&           peer) =0;
    virtual void recvData(const DataSeg&  dataSeg,
                          Peer&           peer) =0;
};

} // namespace

#endif /* MAIN_PROTO_HYCASTPROTO_H_ */
