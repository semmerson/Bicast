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

#include "error.h"
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

    bool write(TcpSock& sock) const {
        return sock.write(pubPath);
    }

    bool read(TcpSock& sock) {
        return sock.read(pubPath);
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

    ProdIndex& operator++() {
        ++index;
        return *this;
    }

    bool write(TcpSock& sock) const {
        return sock.write(index);
    }

    bool read(TcpSock& sock) {
        return sock.read(index);
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

    inline bool operator!=(const DataSegId& rhs) const {
        return !(*this == rhs);
    }

    std::string to_string(const bool withName = false) const;

    size_t hash() const noexcept {
        static std::hash<SegOffset> offHash;
        return prodIndex.hash() ^ offHash(offset);
    }

    bool write(TcpSock& sock) const {
        return prodIndex.write(sock) && sock.write(offset);
    }

    bool read(TcpSock& sock) {
        return prodIndex.read(sock) && sock.read(offset);
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

    bool write(TcpSock& sock) const {
        return sock.write(sec) && sock.write(nsec);
    }

    bool read(TcpSock& sock) {
        return sock.read(sec) && sock.read(nsec);
    }
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
             const ProdSize    size);

    operator bool() const;

    const ProdIndex& getIndex() const;
    const String&    getName() const;
    const ProdSize&  getSize() const;

    bool operator==(const ProdInfo& rhs) const;

    String to_string(bool withName = false) const;

    bool write(TcpSock& sock) const;

    bool read(TcpSock& sock);
};

class Peer;

/// Data segment
class DataSeg final
{
public:
    class Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    // Ethernet - IP header - TCP header - PduId - prodIndex - offset - prodSize
    static constexpr SegSize CANON_DATASEG_SIZE = 1500 - 20 - 20 - 4 - 4 - 4 - 4;

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

    DataSeg(TcpSock&         sock);

    operator bool() const;

    const DataSegId& getId() const noexcept;

    ProdSize getProdSize() const noexcept;

    const char* getData() const noexcept;

    inline SegSize getSize() const {
        return size(getProdSize(), getId().offset);
    }

    inline ProdSize getOffset() const {
        return getId().offset;
    }

    String to_string(bool withName = false) const;

    bool write(TcpSock& sock) const;
};

/******************************************************************************/
// Protocol data units (PDU)

using PduType = unsigned char;
enum class PduId : PduType {
    UNSET,
    NODE_TYPE,
    PUB_PATH_NOTICE,
    PROD_INFO_NOTICE,
    DATA_SEG_NOTICE,
    PROD_INFO_REQUEST,
    DATA_SEG_REQUEST,
    PROD_INFO,
    DATA_SEG
};

/**
 * Class for both notices and requests sent to a remote peer. It exists so that
 * such entities can be handled as a single object for the purpose of argument
 * passing and container element.
 */
struct NoteReq
{
public:
    enum class Id {
        UNSET,
        PROD_INDEX,
        DATA_SEG_ID
    } id;
    union {
        ProdIndex prodIndex;
        DataSegId dataSegId;
    };

    NoteReq()
        : prodIndex()
        , id(Id::UNSET)
    {}

    NoteReq(const NoteReq& noteReq) =default;
    NoteReq(NoteReq&& noteReq) =default;

    NoteReq(const ProdIndex prodIndex)
        : id(Id::PROD_INDEX)
        , prodIndex(prodIndex)
    {}

    NoteReq(const DataSegId dataSegId)
        : id(Id::DATA_SEG_ID)
        , dataSegId(dataSegId)
    {}

    NoteReq& operator=(const NoteReq& noteReq) =default;

    NoteReq& operator=(NoteReq&& noteReq) =default;

    operator bool() const {
        return id != Id::UNSET;
    }

    String to_string() const;

    // `std::hash<NoteReq>()` is also defined
    size_t hash() const noexcept;

    bool operator==(const NoteReq& rhs) const noexcept;
};

/******************************************************************************/
// Receiver/server interfaces:

/// Multicast receiver/server
class McastRcvr
{
public:
    virtual ~McastRcvr() {}
    virtual void recvMcast(const ProdInfo prodInfo) =0;
    virtual void recvMcast(const DataSeg dataSeg) =0;
};

class Peer;

/// Notice receiver/server
class NoticeRcvr
{
public:
    virtual ~NoticeRcvr() {}
    virtual void recvNotice(const PubPath   notice,
                            Peer            peer) =0;
    virtual bool recvNotice(const ProdIndex notice,
                            Peer            peer) =0;
    virtual bool recvNotice(const DataSegId notice,
                            Peer            peer) =0;
};

/// Request receiver/server
class RequestRcvr
{
public:
    virtual ~RequestRcvr() {}
    virtual ProdInfo recvRequest(const ProdIndex request,
                                 Peer            peer) =0;
    virtual DataSeg recvRequest(const DataSegId  request,
                                Peer             peer) =0;
};

/// Data receiver/server
class DataRcvr
{
public:
    virtual ~DataRcvr() {}
    virtual void recvData(const ProdInfo prodInfo,
                          Peer           peer) =0;
    virtual void recvData(const DataSeg  dataSeg,
                          Peer           peer) =0;
};

} // namespace

namespace std {
    template<>
    class hash<hycast::ProdIndex> {
    public:
        size_t operator()(const hycast::ProdIndex& prodIndex) const noexcept {
            return prodIndex.hash();
        }
    };

    template<>
    class hash<hycast::NoteReq> {
    public:
        size_t operator()(const hycast::NoteReq& noteReq) const noexcept {
            return noteReq.hash();
        }
    };
}

#endif /* MAIN_PROTO_HYCASTPROTO_H_ */
