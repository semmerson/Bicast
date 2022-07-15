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

using namespace std::chrono;

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
    static constexpr Type GOOD_P2P_SRVR     = 3;
    static constexpr Type GOOD_P2P_SRVRS    = 4;
    static constexpr Type BAD_P2P_SRVR      = 5;
    static constexpr Type BAD_P2P_SRVRS     = 6;
    static constexpr Type PUB_PATH_NOTICE   = 7;
    static constexpr Type PROD_INFO_NOTICE  = 8;
    static constexpr Type DATA_SEG_NOTICE   = 9;
    static constexpr Type PROD_INFO_REQUEST = 10;
    static constexpr Type DATA_SEG_REQUEST  = 11;
    static constexpr Type PROD_INFO         = 12;
    static constexpr Type DATA_SEG          = 13;
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
        if (!xprt.read(value))
            return false;
        if (value > MAX_PDU_ID)
            throw INVALID_ARGUMENT("value=" + to_string());
        return true;
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

/**
 * Product identifier.
 */
class ProdId : public XprtAble
{
    /**
     * The underlying type used to uniquely identify a product.
     *
     * Using a  hash value to identify a product means that the probability of two or more products
     * having the same hash value in the repository is approximately 1 - e^-(n^2/2d), where n is the
     * number of products and d is the number of possible hash values. For an 8-byte hash value and
     * one hour of the feed with the highest rate of products (NEXRAD3: ~106e3/hr as of 2022-05)
     * this is approximately 3.06e-10. See "Birthday problem" in Wikipedia for details.
     *
     * Alternatively, the probability that an incoming product will have the same hash value as an
     * existing but different product is n/d, which is approximately 5.8e-15 in the above NEXRAD3
     * case. So there's a 50% chance of a collision in approximately 93e3 years.
     *
     * Using the hash of the product name instead of a monotonically increasing unsigned integer
     * means that 1) product names should be unique; and 2) redundant publishers are possible.
     */
    uint64_t id;    ///< Data-product identifier

public:
    ProdId()
        : id(0)
    {}

    explicit ProdId(const String& prodName);

    ProdId(const ProdId& prodId) =default;
    ~ProdId() =default;
    ProdId& operator=(const ProdId& rhs) =default;

    std::string to_string() const noexcept;

    size_t hash() const noexcept {
        static auto myHash = std::hash<decltype(id)>{};
        return myHash(id);
    }

    bool operator==(const ProdId& rhs) const noexcept {
        return id == rhs.id;
    }

    bool operator<(const ProdId& rhs) const noexcept {
        return id < rhs.id;
    }

    bool write(Xprt xprt) const override {
        return xprt.write(id);
    }

    bool read(Xprt xprt) override {
        return xprt.read(id);
    }
};

/// Timestamp
class Timestamp : public XprtAble
{
public:
    using Clock     = system_clock;
    using TimePoint = Clock::time_point;
    using Duration  = Clock::duration;

    Timestamp()
        : timePoint(Clock::now())
    {}

    Timestamp(const TimePoint& timePoint)
        : timePoint(timePoint)
    {}

    Timestamp(const TimePoint&& timePoint)
        : timePoint(timePoint)
    {}

    Timestamp(const struct timespec& time)
        : timePoint(Clock::from_time_t(time.tv_sec) + nanoseconds(time.tv_nsec))
    {}

    const TimePoint getTimePoint() const noexcept {
        return timePoint;
    }

    operator TimePoint() {
        return timePoint;
    }

    bool operator==(const Timestamp& rhs) const noexcept {
        return timePoint == rhs.timePoint;
    }

    bool operator<(const Timestamp& rhs) const noexcept {
        return timePoint < rhs.timePoint;
    }

    String to_string() const noexcept;

    bool write(Xprt xprt) const override;

    bool read(Xprt xprt) override;

private:
    TimePoint timePoint;
};

/// Data-segment identifier
struct DataSegId : public XprtAble
{
    ProdId    prodId; ///< Product index
    SegOffset offset; ///< Offset of data segment in bytes

    DataSegId()
        : prodId()
        , offset{0}
    {}

    DataSegId(const ProdId    prodId,
              const SegOffset offset)
        : prodId(prodId)
        , offset{offset}
    {}

    inline bool operator==(const DataSegId rhs) const {
        return (prodId == rhs.prodId) && (offset == rhs.offset);
    }

    inline bool operator!=(const DataSegId rhs) const {
        return !(*this == rhs);
    }

    std::string to_string(const bool withName = false) const;

    size_t hash() const noexcept {
        static std::hash<SegOffset> offHash;
        return prodId.hash() ^ offHash(offset);
    }

    bool write(Xprt xprt) const override {
        auto success = prodId.write(xprt);
        if (success) {
            success = xprt.write(offset);
        }
        return success;
    }

    bool read(Xprt xprt) override {
        auto success = prodId.read(xprt);
        if (success)  {
            success = xprt.read(offset);
        }
        return success;
    }
};

/// Handle class for product information
struct ProdInfo : public XprtAble
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    static constexpr PduId::Type pduId = PduId::PROD_INFO;

    ProdInfo();

    /**
     * The creation-time of the product will be the current time.
     *
     * @param[in] prodId  Product ID
     * @param[in] name    Name of product
     * @param[in] size    Size of product in bytes
     */
    ProdInfo(const ProdId       prodId,
             const std::string& name,
             const ProdSize     size);

    operator bool() const noexcept;

    const ProdId&    getId() const;
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
 * Class for both notices and requests sent to a remote peer. It exists so that such entities can be
 * handled as a single object for the purpose of argument passing and container element.
 */
struct DatumId
{
public:
    enum class Id {
        UNSET,
        GOOD_P2P_SRVR,
        GOOD_P2P_SRVRS,
        BAD_P2P_SRVR,
        BAD_P2P_SRVRS,
        PROD_INDEX,
        DATA_SEG_ID
    } id;
    union {
        SockAddr  srvrAddr;
        Tracker   tracker;
        ProdId    prodId;
        DataSegId dataSegId;
    };

    DatumId() noexcept
        : prodId()
        , id(Id::UNSET)
    {}

    DatumId(const SockAddr srvrAddr, const bool isGood = true) noexcept
        : id(isGood ? Id::GOOD_P2P_SRVR : Id::BAD_P2P_SRVR)
        , srvrAddr(srvrAddr)
    {}

    DatumId(const Tracker tracker, const bool isGood = true) noexcept
        : id(isGood ? Id::GOOD_P2P_SRVRS : Id::BAD_P2P_SRVRS)
        , tracker(tracker)
    {}

    explicit DatumId(const ProdId prodId) noexcept
        : id(Id::PROD_INDEX)
        , prodId(prodId)
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

    bool equals(const ProdId prodId) {
        return id == Id::PROD_INDEX && this->prodId == prodId;
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
    virtual bool recvNotice(const ProdId notice,
                            SockAddr        rmtAddr) =0;
    virtual bool recvNotice(const DataSegId notice,
                            SockAddr        rmtAddr) =0;
};

/// Request receiver/server interface
class RequestRcvr
{
public:
    virtual ~RequestRcvr() {}
    virtual ProdInfo recvRequest(const ProdId request,
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
    virtual ProdInfo recvRequest(const ProdId request) =0;
    virtual DataSeg recvRequest(const DataSegId request) =0;
};

/// Subscribing peer's receiver interface
class SubRcvr : public PubRcvr
{
public:
    virtual ~SubRcvr() {};
    virtual bool recvNotice(const ProdId notice) =0;
    virtual bool recvNotice(const DataSegId notice) =0;
    virtual bool recvData(const Tracker  tracker) =0;
    virtual bool recvData(const SockAddr srvrAddr) =0;
    virtual void recvData(const ProdInfo prodInfo) =0;
    virtual void recvData(const DataSeg  dataSeg) =0;
};

} // namespace

namespace std {
    template<>
    struct hash<hycast::ProdId> {
        size_t operator()(const hycast::ProdId& prodId) const noexcept {
            return prodId.hash();
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
