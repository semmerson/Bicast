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

#ifndef MAIN_PROTO_HYCASTPROTO_H_
#define MAIN_PROTO_HYCASTPROTO_H_

#include "Socket.h"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <time.h>

namespace hycast {

// Convenience types
using Thread = std::thread;
using Mutex  = std::mutex;
using Guard  = std::lock_guard<Mutex>;
using Lock   = std::unique_lock<Mutex>;
using Cond   = std::condition_variable;
using String = std::string;

/******************************************************************************/
// PDU payloads

/// Path-to-publisher notice
class PubPath
{
    bool pubPath; // Something is a path to the publisher

public:
    /**
     * NB: Implicit construction.
     * @param[in] pubPath  Whether something is path to publisher
     */
    PubPath(const bool pubPath)
        : pubPath(pubPath)
    {}

    PubPath()
        : PubPath(false)
    {}

    PubPath(const PubPath& pubPath) =default;
    ~PubPath() =default;
    PubPath& operator=(const PubPath& rhs) =default;

    operator bool() const {
        return pubPath;
    }

    std::string to_string(const bool withName) const {
        return withName
                ? "PubPath{" + std::to_string(pubPath) + "}"
                : std::to_string(pubPath);
    }
};

class ProdIndex
{
public:
    using Type = uint32_t;

private:
    Type index;    ///< Index of data-product

public:
    /**
     * NB: Implicit construction.
     * @param[in] index  Index of data-product
     */
    ProdIndex(const Type index)
        : index(index)
    {}

    ProdIndex()
        : ProdIndex(0)
    {}

    ProdIndex(const ProdIndex& prodIndex) =default;
    ~ProdIndex() =default;
    ProdIndex& operator=(const ProdIndex& rhs) =default;

    operator Type() const {
        return index;
    }

    std::string to_string(const bool withName = false) const {
        return withName
                ? "ProdIndex{" + std::to_string(index) + "}"
                : std::to_string(index);
    }

    size_t hash() const noexcept {
        static std::hash<Type> indexHash;
        return indexHash(index);
    }
};
using ProdSize  = uint32_t;    ///< Size of product in bytes
using SegSize   = uint16_t;    ///< Data-segment size in bytes
using SegOffset = ProdSize;    ///< Offset of data-segment in bytes

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

    std::string to_string(const bool withName = false) const;

    size_t hash() const noexcept {
        static std::hash<SegOffset> offHash;
        return prodIndex.hash() ^ offHash(offset);
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

    /**
     * Returns string representation as "YYYY-MM-DDThh:mm:ss.nnnnnnZ"
     */
    std::string to_string(bool withName = false) const;
};

/// Product information
struct ProdInfo
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    ProdInfo();

    ProdInfo(const ProdIndex   index,
             const std::string name,
             const ProdSize    size,
             const Timestamp   created);

    ProdInfo(const ProdIndex   index,
             const std::string name,
             const ProdSize    size);

    const ProdIndex& getProdIndex() const;
    const String&    getName() const;
    const ProdSize&  getProdSize() const;
    const Timestamp& getTimestamp() const;

    bool operator==(const ProdInfo& rhs) const;

    String to_string(bool withName = false) const;
};

class Peer;

/// Data segment
class DataSeg
{
public:
    class Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    // Ethernet - IP header - TCP header - PduId - prodIndex - offset - prodSize
    static const SegSize CANON_DATASEG_SIZE = 1500 - 20 - 20 - 4 - 4 - 4 - 4;

    inline static SegSize size(ProdSize prodSize, SegOffset offset) noexcept {
        SegSize nbytes = prodSize - offset;
        return (nbytes > CANON_DATASEG_SIZE)
                ? CANON_DATASEG_SIZE
                : nbytes;
    }

    DataSeg();

    DataSeg(const DataSegId& segId,
            const ProdSize   prodSize,
            const char*      data);

    DataSeg(const DataSegId& segId,
            const ProdSize   prodSize,
            TcpSock&         sock);

    const DataSegId& segId() const noexcept;

    ProdSize prodSize() const noexcept;

    const char* data() const noexcept;

    inline SegSize size() const {
        return size(prodSize(), segId().offset);
    }

    String to_string(bool withName = false) const;
};

/******************************************************************************/
// Protocol data units (PDU)

using PduType = unsigned char;
enum class PduId : PduType {
    PUB_PATH_NOTICE,
    PROD_INFO_NOTICE,
    DATA_SEG_NOTICE,
    PROD_INFO_REQUEST,
    DATA_SEG_REQUEST,
    PROD_INFO,
    DATA_SEG
};

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
    virtual void recvNotice(const PubPath  notice,
                            Peer           peer) =0;
    virtual void recvNotice(const ProdIndex notice,
                            Peer            peer) =0;
    virtual void recvNotice(const DataSegId&  notice,
                            Peer              peer) =0;
};

/// Request receiver/server
class RequestRcvr
{
public:
    virtual ~RequestRcvr() {}
    virtual void recvRequest(const ProdIndex request,
                             Peer            peer) =0;
    virtual void recvRequest(const DataSegId& request,
                             Peer             peer) =0;
};

/// Data receiver/server
class DataRcvr
{
public:
    virtual ~DataRcvr() {}
    virtual void recvData(const ProdInfo& prodInfo,
                          Peer            peer) =0;
    virtual void recvData(const DataSeg&  dataSeg,
                          Peer            peer) =0;
};

/// Peer-to-peer manager interface
class P2pMgr
{
public:
    virtual ~P2pMgr() {}
    virtual void offline(Peer peer) =0;
    virtual void reassigned(const ProdIndex  notice,
                            Peer             peer) =0;
    virtual void reassigned(const DataSegId& notice,
                            Peer             peer) =0;
};

} // namespace

#endif /* MAIN_PROTO_HYCASTPROTO_H_ */
