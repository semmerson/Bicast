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
#include "Tracker.h"
#include "Xprt.h"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <time.h>

namespace hycast {

class PubRepo;
class SubRepo;

// Convenience types
using Thread = std::thread;
using Mutex  = std::mutex;
using Guard  = std::lock_guard<Mutex>;
using Lock   = std::unique_lock<Mutex>;
using Cond   = std::condition_variable;
using String = std::string;

constexpr uint8_t PROTOCOL_VERSION = 1;

class P2pMgr;
class SubP2pMgr;

// Protocol data unit (PDU) identifiers
class PduId : public XprtAble
{
    uint16_t value;

public:
    using Type = decltype(value);

    static constexpr Type UNSET             = 0;
    static constexpr Type PROTOCOL_VERSION  = 1;
    static constexpr Type IS_PUBLISHER      = 2;
    static constexpr Type PEER_SRVR_ADDRS   = 3;
    static constexpr Type PUB_PATH_NOTICE   = 4;
    static constexpr Type PROD_INFO_NOTICE  = 5;
    static constexpr Type DATA_SEG_NOTICE   = 6;
    static constexpr Type PROD_INFO_REQUEST = 7;
    static constexpr Type DATA_SEG_REQUEST  = 8;
    static constexpr Type PEER_SRVR_ADDR    = 9;
    static constexpr Type PROD_INFO         = 10;
    static constexpr Type DATA_SEG          = 11;
    static constexpr Type MAX_PDU_ID        = DATA_SEG;

    /**
     * Constructs.
     *
     * @param[in] value            PDU ID value
     * @throws    IllegalArgument  `value` is unsupported
     */
    PduId(Type value)
        : value(value)
    {
        if (value > MAX_PDU_ID)
            throw INVALID_ARGUMENT("value=" + to_string());
    }

    PduId()
        : value(UNSET)
    {}

    operator bool() const noexcept {
        return value != UNSET;
    }

    inline String to_string() const {
        return std::to_string(value);
    }

    inline operator Type() const noexcept {
        return value;
    }

    inline bool operator==(const PduId rhs) const noexcept {
        return value == rhs.value;
    }

    inline bool operator==(const Type rhs) const noexcept {
        return value == rhs;
    }

    inline bool write(Xprt xprt) const {
        return xprt.write(value);
    }

    inline bool read(Xprt xprt) {
        return xprt.read(value);
    }
};

/******************************************************************************/
// PDU payloads

using ProdSize  = uint32_t;    ///< Size of product in bytes
using SegSize   = uint16_t;    ///< Data-segment size in bytes
using SegOffset = ProdSize;    ///< Offset of data-segment in bytes

class Xprt;

/// Information on a data feed
struct FeedInfo : public XprtAble
{
    SockAddr mcastGroup;  ///< Multicast group address
    InetAddr mcastSource; ///< Multicast source address
    SegSize  segSize;     ///< Canonical data-segment size in bytes

    /**
     * Copies this instance to a transport.
     *
     * @param[in] xprt     Transport
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool write(Xprt xprt) const override;

    /**
     * Initializes this instance from a transport.
     *
     * @param[in] xprt     Transport
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool read(Xprt xprt) override;
};

/// Path-to-publisher notice
class PubPath : public XprtAble
{
    bool pubPath; // Something is or has a path to the publisher

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

    bool write(Xprt xprt) const override {
        return xprt.write(pubPath);
    }

    bool read(Xprt xprt) override {
        return xprt.read(pubPath);
    }
};

class ProdIndex : public XprtAble
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

    bool write(Xprt xprt) const override {
        return xprt.write(index);
    }

    bool read(Xprt xprt) override {
        return xprt.read(index);
    }
};

/// Data-segment identifier
struct DataSegId : public XprtAble
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

    inline bool operator==(const DataSegId rhs) const {
        return (prodIndex == rhs.prodIndex) && (offset == rhs.offset);
    }

    inline bool operator!=(const DataSegId rhs) const {
        return !(*this == rhs);
    }

    std::string to_string(const bool withName = false) const;

    size_t hash() const noexcept {
        static std::hash<SegOffset> offHash;
        return prodIndex.hash() ^ offHash(offset);
    }

    bool write(Xprt xprt) const override {
        auto success = prodIndex.write(xprt);
        if (success) {
            success = xprt.write(offset);
        }
        return success;
    }

    bool read(Xprt xprt) override {
        auto success = prodIndex.read(xprt);
        if (success)  {
            success = xprt.read(offset);
        }
        return success;
    }
};

/// Timestamp
struct Timestamp : public XprtAble
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

    bool write(Xprt xprt) const override {
        return xprt.write(sec) && xprt.write(nsec);
    }

    bool read(Xprt xprt) override {
        return xprt.read(sec) && xprt.read(nsec);
    }
};

/// Handle class for roduct information
struct ProdInfo : public XprtAble
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    static constexpr PduId::Type pduId = PduId::PROD_INFO;

    ProdInfo();

    ProdInfo(const ProdIndex   index,
             const std::string name,
             const ProdSize    size);

    operator bool() const noexcept;

    const ProdIndex& getIndex() const;
    const ProdIndex& getId() const {
        return getIndex();
    }
    const String&    getName() const;
    const ProdSize&  getSize() const;

    bool operator==(const ProdInfo rhs) const;

    String to_string(bool withName = false) const;

    bool write(Xprt xprt) const override;

    bool read(Xprt xprt) override;
};

class Peer;

/// Data segment
class DataSeg final : public XprtAble
{
public:
    class Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Sets the maximum size of a data-segment.
     *
     * @param[in] maxSegSize       Maximum data-segment size in bytes
     * @return                     Previous value
     * @throw     InvalidArgument  Argument is not positive
     */
    static SegSize setMaxSegSize(const SegSize maxSegSize) noexcept;

    /**
     * Gets the maximum size of a data-segment.
     *
     * @return  Maximum size of a data-segment in bytes
     */
    static SegSize getMaxSegSize() noexcept;

    /**
     * Returns the size of a given data-segment.
     *
     * @param[in] prodSize  Size of the data-product in bytes
     * @param[in] offset    Offset to the data-segment in bytes
     * @return              Size of the data-segment in bytes
     */
    static SegSize size(
            const ProdSize  prodSize,
            const SegOffset offset) noexcept;

    /**
     * Returns the number of data-segments in a product.
     *
     * @param[in] prodSize  Size of the product in bytes
     * @return              Number of data-segments in the product
     */
    static ProdSize numSegs(const ProdSize prodSize) noexcept;

    static constexpr PduId::Type pduId = PduId::DATA_SEG;

    DataSeg();

    /**
     * Constructs.
     *
     * @param[in] segId     Segment identifier
     * @param[in] prodSize  Product size in bytes
     * @param[in] data      Segment data. Caller may free.
     */
    DataSeg(const DataSegId segId,
            const ProdSize  prodSize,
            const char*     data);

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

    bool operator==(const DataSeg& rhs) const;

    String to_string(bool withName = false) const;

    bool write(Xprt xprt) const override;

    bool read(Xprt xprt) override;
};

/******************************************************************************/

/**
 * Interface for data-products.
 */
class Product
{
public:
    /**
     * Returns a data-product that resides in memory.
     *
     * @param[in] prodInfo  Product information
     * @param[in] data      Product data. Amount must be consonant with product-
     *                      information. Must exist until destructor is called.
     * @return              Memory-resident data-product
     */
    static std::shared_ptr<Product> create(const ProdInfo prodInfo,
                                           const char*    data);

    virtual ~Product() noexcept =default;

    virtual ProdInfo getProdInfo() const =0;

    virtual char* getData(const ProdSize offset,
                          const SegSize  nbytes);
};

/******************************************************************************/

/**
 * Class for both notices and requests sent to a remote peer. It exists so that
 * such entities can be handled as a single object for the purpose of argument
 * passing and container element.
 */
struct DatumId
{
public:
    enum class Id {
        UNSET,
        PEER_SRVR_ADDR,
        TRACKER,
        PROD_INDEX,
        DATA_SEG_ID
    } id;
    union {
        SockAddr  srvrAddr;
        Tracker   tracker;
        ProdIndex prodIndex;
        DataSegId dataSegId;
    };

    DatumId() noexcept
        : prodIndex()
        , id(Id::UNSET)
    {}

    explicit DatumId(const SockAddr srvrAddr) noexcept
        : id(Id::PEER_SRVR_ADDR)
        , srvrAddr(srvrAddr)
    {}

    explicit DatumId(const Tracker tracker) noexcept
        : id(Id::TRACKER)
        , tracker(tracker)
    {}

    explicit DatumId(const ProdIndex prodIndex) noexcept
        : id(Id::PROD_INDEX)
        , prodIndex(prodIndex)
    {}

    explicit DatumId(const DataSegId dataSegId) noexcept
        : id(Id::DATA_SEG_ID)
        , dataSegId(dataSegId)
    {}

    DatumId(const DatumId& datumId) noexcept {
        ::memcpy(this, &datumId, sizeof(DatumId));
    }

    ~DatumId() noexcept {
    }

    Id getType() const noexcept {
        return id;
    }

    bool equals(const ProdIndex prodIndex) {
        return id == Id::PROD_INDEX && this->prodIndex == prodIndex;
    }

    bool equals(const DataSegId dataSegId) {
        return id == Id::DATA_SEG_ID && this->dataSegId == dataSegId;
    }

    DatumId& operator=(const DatumId& rhs) noexcept {
        ::memcpy(this, &rhs, sizeof(DatumId));
        return *this;
    }

    operator bool() const {
        return id != Id::UNSET;
    }

    String to_string() const;

    // `std::hash<DatumId>()` is also defined
    size_t hash() const noexcept;

    bool operator==(const DatumId& rhs) const noexcept;
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

/// Notice receiver/server interface
class NoticeRcvr
{
public:
    virtual ~NoticeRcvr() {}
    virtual bool recvNotice(const ProdIndex notice,
                            SockAddr        rmtAddr) =0;
    virtual bool recvNotice(const DataSegId notice,
                            SockAddr        rmtAddr) =0;
};

/// Request receiver/server interface
class RequestRcvr
{
public:
    virtual ~RequestRcvr() {}
    virtual ProdInfo recvRequest(const ProdIndex request,
                                 const SockAddr  rmtAddr) =0;
    virtual DataSeg recvRequest(const DataSegId request,
                                const SockAddr  rmtAddr) =0;
};

/// Data receiver/server interface
class DataRcvr
{
public:
    virtual ~DataRcvr() {}
    virtual void recvData(
            const Tracker  tracker,
            const SockAddr rmtAddr) =0;
    virtual void recvData(
            const SockAddr srvrAddr,
            const SockAddr rmtAddr) =0;
    virtual void recvData(const ProdInfo prodInfo,
                          const SockAddr rmtAddr) =0;
    virtual void recvData(const DataSeg  dataSeg,
                          const SockAddr rmtAddr) =0;
};

/// Publishing peer's receiver interface
class PubRcvr
{
public:
    virtual ~PubRcvr() {};
    virtual ProdInfo recvRequest(const ProdIndex request) =0;
    virtual DataSeg recvRequest(const DataSegId request) =0;
};

/// Subscribing peer's receiver interface
class SubRcvr : public PubRcvr
{
public:
    virtual ~SubRcvr() {};
    virtual bool recvNotice(const ProdIndex notice) =0;
    virtual bool recvNotice(const DataSegId notice) =0;
    virtual bool recvData(const Tracker  tracker) =0;
    virtual bool recvData(const SockAddr srvrAddr) =0;
    virtual void recvData(const ProdInfo prodInfo) =0;
    virtual void recvData(const DataSeg  dataSeg) =0;
};

} // namespace

namespace std {
    template<>
    struct hash<hycast::ProdIndex> {
        size_t operator()(const hycast::ProdIndex& prodIndex) const noexcept {
            return prodIndex.hash();
        }
    };

    template<>
    struct hash<hycast::DatumId> {
        size_t operator()(const hycast::DatumId& datumId) const noexcept {
            return datumId.hash();
        }
    };
}

#endif /* MAIN_PROTO_HYCASTPROTO_H_ */
