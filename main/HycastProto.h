/**
 * This file declares the types used in the Hycast protocol.
 * 
 * @file:   HycastProto.h
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

#include "CommonTypes.h"
#include "error.h"
#include "Socket.h"
#include "Tracker.h"
#include "Xprt.h"

#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <time.h>
#include <unordered_set>

namespace hycast {

using namespace std::chrono;

class PubRepo;
class SubRepo;

constexpr uint8_t PROTOCOL_VERSION = 1; ///< Protocol version

class P2pMgr;
class SubP2pMgr;

/// Protocol data unit (PDU) identifiers
class PduId : public XprtAble
{
    uint16_t value;

public:
    using Type = decltype(value); ///< Underlying type of protocol data unit identifier

    /// Types of protocol data units
    enum id : Type {
        UNSET,
        PROTOCOL_VERSION,  ///< Protocol version
        IS_PUBLISHER,      ///< Is the peer the publisher?
        AM_PUB_PATH,       ///< The peer has a path to the publisher
        AM_NOT_PUB_PATH,   ///< The peer does not have a path to the publisher
        GOOD_P2P_SRVR,     ///< Here's a good P2P server
        GOOD_P2P_SRVRS,    ///< Here are good P2P servers
        BAD_P2P_SRVR,      ///< Here's a bad P2P server
        BAD_P2P_SRVRS,     ///< Here are bad P2P servers
        PUB_PATH_NOTICE,   ///< Does the peer have a path to the publisher?
        PROD_INFO_NOTICE,  ///< Here's a notice about available information on a data product
        DATA_SEG_NOTICE,   ///< Here's a notice about an available data segment
        BACKLOG_REQUEST,   ///< Request for anything missed since the end of the previous session
        PROD_INFO_REQUEST, ///< Request for product information
        DATA_SEG_REQUEST,  ///< Request for a data segment
        PROD_INFO,         ///< Product information
        DATA_SEG,          ///< Data segment
        MAX_PDU_ID = DATA_SEG
    };

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

    /**
     * Constructs.
     * @param[in] xprt  Transport from which to read the PDU ID
     */
    PduId(Xprt xprt) {
        if (!xprt.read(value))
            throw EOF_ERROR("Couldn't read value");
        if (value > MAX_PDU_ID)
            throw INVALID_ARGUMENT("value=" + to_string());
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    inline String to_string() const {
        return std::to_string(value);
    }

    /**
     * Returns the underlying ID.
     */
    inline operator Type() const noexcept {
        return value;
    }

    /**
     * Tests for equality with another instance.
     * @param[in] rhs      The other, right-hand-side instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    inline bool operator==(const PduId rhs) const noexcept {
        return value == rhs.value;
    }

    /**
     * Tests for equality with an underlying ID.
     * @param[in] rhs      The underlying, right-hand-side ID
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    inline bool operator==(const enum id rhs) const noexcept {
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
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(Xprt xprt) const override;

    /**
     * Initializes this instance from a transport.
     *
     * @param[in] xprt     Transport
     * @retval    true     Success
     * @retval    false    Connection lost
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

    /**
     * Copy constructs.
     * @param[in] pubPath  The other instance
     */
    PubPath(const PubPath& pubPath) =default;
    ~PubPath() =default;
    /**
     * Copy assigns.
     * @param[in] rhs  The other instance
     * @return         A reference to this just-assigned instance
     */
    PubPath& operator=(const PubPath& rhs) =default;

    /**
     * Indicates if this instance is valid.
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const {
        return pubPath;
    }

    /**
     * Returns the string representation of this instance.
     * @param[in] withName  Should the name of this class be included?
     * @return              The string representation of this instance
     */
    std::string to_string(const bool withName) const {
        return withName
                ? "PubPath{" + std::to_string(pubPath) + "}"
                : std::to_string(pubPath);
    }

    /**
     * Writes itself to a transport.
     * @param[in] xprt  The transport
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(Xprt xprt) const override {
        return xprt.write(pubPath);
    }

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
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

    /**
     * Copy constructs.
     * @param[in] prodId  The other instance
     */
    ProdId(const ProdId& prodId) =default;
    ~ProdId() =default;
    /**
     * Copy assigns.
     * @param[in] rhs  The other instance
     * @return         A reference to this just-assigned instance
     */
    ProdId& operator=(const ProdId& rhs) =default;

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    std::string to_string() const noexcept;

    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
    size_t hash() const noexcept {
        static auto myHash = std::hash<decltype(id)>{};
        return myHash(id);
    }

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs      The other instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    bool operator==(const ProdId& rhs) const noexcept {
        return id == rhs.id;
    }

    /**
     * Indicates if this instance is less than another.
     * @param[in] rhs  The other instance
     * @retval    true     This instance is less than the other
     * @retval    false    This instance is not less than the other
     */
    bool operator<(const ProdId& rhs) const noexcept {
        return id < rhs.id;
    }

    /**
     * Writes itself to a transport.
     * @param[in] xprt  The transport
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(Xprt xprt) const override {
        return xprt.write(id);
    }

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool read(Xprt xprt) override {
        return xprt.read(id);
    }
};

} // namespace

namespace std {
    /// The hash code class for a data product's identifier.
    template<>
    struct hash<hycast::ProdId> {
        /**
         * Returns the hash code of a product identifier.
         * @param prodId  The product identifier
         * @return        The hash code of the product identifier
         */
        size_t operator()(const hycast::ProdId& prodId) const noexcept {
            return prodId.hash();
        }
    };
} // namespace

namespace hycast {

/// Data-segment identifier
struct DataSegId : public XprtAble
{
    ProdId    prodId; ///< Product index
    SegOffset offset; ///< Offset of data segment in bytes

    DataSegId()
        : prodId()
        , offset{0}
    {}

    /**
     * Constructs.
     * @param[in] prodId  Product ID
     * @param[in] offset  Offset to the start of the segment in bytes
     */
    DataSegId(const ProdId    prodId,
              const SegOffset offset)
        : prodId(prodId)
        , offset{offset}
    {}

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs      The other instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    inline bool operator==(const DataSegId rhs) const {
        return (prodId == rhs.prodId) && (offset == rhs.offset);
    }

    /**
     * Indicates if this instance is not equal to another.
     * @param[in] rhs      The other instance
     * @retval    true     This instance is not equal to the other
     * @retval    false    This instance is equal to the other
     */
    inline bool operator!=(const DataSegId rhs) const {
        return !(*this == rhs);
    }

    /**
     * Returns the string representation of this instance.
     * @param[in] withName  Should the name of this class be included?
     * @return              The string representation of this instance
     */
    std::string to_string(const bool withName = false) const;

    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
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
    /// Smart pointer to the implementation
    std::shared_ptr<Impl> pImpl;

public:
    /// The type of product data unit for this class
    static constexpr PduId::Type pduId = PduId::PROD_INFO;

    ProdInfo() =default;

    /**
     * Constructs.
     *
     * @param[in] prodId      Product ID
     * @param[in] name        Name of product
     * @param[in] size        Size of product in bytes
     * @param[in] createTime  When the product was created
     */
    ProdInfo(const ProdId        prodId,
             const std::string&  name,
             const ProdSize      size,
             const SysTimePoint& createTime = SysClock::now());

    /**
     * Constructs.
     *
     * @param[in] name        Name of product
     * @param[in] size        Size of product in bytes
     * @param[in] createTime  When the product was created
     */
    ProdInfo(const std::string&  name,
             const ProdSize      size,
             const SysTimePoint& createTime = SysClock::now());

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Returns the product's ID.
     * @return The product's ID
     */
    const ProdId&       getId() const;
    /**
     * Returns the product's name.
     * @return The product's name
     */
    const String&       getName() const;
    /**
     * Returns the product's size in bytes.
     * @return The product's size in bytes
     */
    const ProdSize&     getSize() const;
    /**
     * Returns the product's creation time.
     * @return The product's creation time
     */
    const SysTimePoint& getCreateTime() const;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs      The other instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    bool operator==(const ProdInfo rhs) const;

    /**
     * Returns the string representation of this instance.
     * @param[in] withName  Should the name of this class be included?
     * @return              The string representation of this instance
     */
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

    /**
     * Returns the origin-0 index of a data-segment.
     *
     * @param[in] offset  Offset, in bytes, of the data-segment
     * @return            Origin-0 index of the segment
     */
    static ProdSize getSegIndex(const ProdSize offset) noexcept;

    /// The type of protocol data unit for this class
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

    /**
     * Indicates if this instance is valid.
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Returns the ID of this instance.
     * @return The ID of this instance
     */
    const DataSegId& getId() const noexcept;

    /**
     * Returns the size of the associated product.
     * @return The size of the associated product in bytes
     */
    ProdSize getProdSize() const noexcept;

    /**
     * Returns the data of this instance.
     * @return The data of this instance
     */
    const char* getData() const noexcept;

    /**
     * Returns the amount of data in this instance.
     * @return The amount of data in this instance in bytes
     */
    inline SegSize getSize() const {
        return size(getProdSize(), getId().offset);
    }

    /**
     * Returns the offset to the start of this segment in the product.
     * @return The offset, in bytes, to the start of this segment in the product
     */
    inline ProdSize getOffset() const {
        return getId().offset;
    }

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs      The other instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    bool operator==(const DataSeg& rhs) const;

    /**
     * Returns the string representation of this instance.
     * @param[in] withName  Should the name of this class be included?
     * @return              The string representation of this instance
     */
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

    /**
     * Returns information on the product.
     * @return Information on the product
     */
    virtual ProdInfo getProdInfo() const =0;

    /**
     * Returns a segment of the product's data.
     * @param[in] offset  Offset, in bytes, to the start of the segment
     * @param[in] nbytes  Number of bytes to return
     * @return            Pointer to the start of the segment
     */
    virtual char* getData(const ProdSize offset,
                          const SegSize  nbytes);
};

/******************************************************************************/

/**
 * Class for both notices and requests sent to a remote peer. It exists so that such entities can be
 * handled as a single object for the purpose of argument passing and container element.
 */
struct Notice
{
public:
    /// Identifier of the type of notice
    enum class Id {
        UNSET,
        AM_PUB_PATH,
        GOOD_P2P_SRVR,
        GOOD_P2P_SRVRS,
        BAD_P2P_SRVR,
        BAD_P2P_SRVRS,
        PROD_INDEX,
        DATA_SEG_ID
    } id; ///< Identifier of the type of notice
    union {
        bool      amPubPath;
        SockAddr  srvrAddr;
        Tracker   tracker;
        ProdId    prodId;
        DataSegId dataSegId;
    };

    Notice() noexcept
        : prodId()
        , id(Id::UNSET)
    {}

    /**
     * Constructs a notice about being a path to the publisher.
     * @param[in] amPubPath  Does this instance have a path to the publisher?
     */
    explicit Notice(const bool amPubPath) noexcept
        : id(Id::AM_PUB_PATH)
        , amPubPath(amPubPath)
    {}

    /**
     * Constructs a notice about a P2P server address.
     * @param[in] srvrAddr  The P2P server address
     * @param[in] isGood    Is the address a good one?
     */
    explicit Notice(const SockAddr srvrAddr, const bool isGood = true) noexcept
        : id(isGood ? Id::GOOD_P2P_SRVR : Id::BAD_P2P_SRVR)
        , srvrAddr(srvrAddr)
    {}

    /**
     * Constructs.
     * @param[in] tracker  The tracker to be in the notice
     * @param[in] isGood   Are the P2P server socket addresses good?
     */
    explicit Notice(const Tracker tracker, const bool isGood = true) noexcept
        : id(isGood ? Id::GOOD_P2P_SRVRS : Id::BAD_P2P_SRVRS)
        , tracker(tracker)
    {}

    /**
     * Constructs a notice about an available product.
     * @param[in] prodId  The product's ID
     */
    explicit Notice(const ProdId prodId) noexcept
        : id(Id::PROD_INDEX)
        , prodId(prodId)
    {}

    /**
     * Constructs a notice about an available data segment.
     * @param[in] dataSegId The data segment's ID
     */
    explicit Notice(const DataSegId dataSegId) noexcept
        : id(Id::DATA_SEG_ID)
        , dataSegId(dataSegId)
    {}

    /**
     * Constructs a notice about an available datum.
     * @param[in] datumId  The datum's ID
     */
    Notice(const Notice& datumId) noexcept {
        ::memcpy(this, &datumId, sizeof(Notice));
    }

    ~Notice() noexcept {
    }

    /**
     * Returns the type of the notice.
     * @return The type of the notice
     */
    Id getType() const noexcept {
        return id;
    }

    /**
     * Indicates if this instance is about a given product.
     * @param[in] prodId   The product's ID
     * @retval    true     This instance is about the given product
     * @retval    false    This instance is not about the given product
     */
    bool equals(const ProdId prodId) {
        return id == Id::PROD_INDEX && this->prodId == prodId;
    }

    /**
     * Indicates if this instance is about a given data segment.
     * @param[in] segId    The data segment's ID
     * @retval    true     This instance is about the given data segment
     * @retval    false    This instance is not about the given data segment
     */
    bool equals(const DataSegId segId) {
        return id == Id::DATA_SEG_ID && dataSegId == segId;
    }

    /**
     * Copy assigns.
     * @param[in] rhs  The other instance
     * @return         A reference to this just-assigned instance
     */
    Notice& operator=(const Notice& rhs) noexcept {
        ::memcpy(this, &rhs, sizeof(Notice));
        return *this;
    }

    /**
     * Indicates if this instance is valid.
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const {
        return id != Id::UNSET;
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    String to_string() const;

    // `std::hash<DatumId>()` is also defined
    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
    size_t hash() const noexcept;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs      The other instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    bool operator==(const Notice& rhs) const noexcept;
};

/******************************************************************************/

/// A set of product identifiers
class ProdIdSet : public XprtAble
{
public:
    class Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    //using iterator = std::iterator<std::forward_iterator_tag, const ProdId>;
    /// Iterator type
    using iterator = std::unordered_set<ProdId>::iterator; // HACK!

    /**
     * Constructs.
     *
     * @param[in] n  Initial capacity
     */
    ProdIdSet(const size_t n = 0);

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    String to_string() const;

    /**
     * Subtracts (i.e., erases) all identifiers in another set.
     *
     * @param[in] rhs  The other set
     */
    void subtract(const ProdIdSet rhs);

    bool write(Xprt xprt) const;

    bool read(Xprt xprt);

    /**
     * Returns the number of data product identifiers.
     * @return The number of data product identifiers
     */
    size_t size() const;

    /**
     * Indicates whether or not a given product identifier exists in the set.
     * @param[in] prodId  The product's ID
     * @retval    1       The product ID exists
     * @retval    0       The product ID does not exist
     */
    size_t count(const ProdId prodId) const;

    /**
     * Inserts a product's ID into the set.
     * @param[in] prodId  The product's ID
     */
    void insert(const ProdId prodId) const;

    /**
     * Returns an iterator to the beginning of the set.
     * @return An iterator to the beginning of the set
     */
    iterator begin() const;

    /**
     * Returns an iterator to just past the end of the set.
     * @return An iterator to just past the end of the set
     */
    iterator end() const;

    /**
     * Clears the set of elements.
     */
    void clear() const;
};

/******************************************************************************/
// Receiver/server interfaces:

/// Multicast receiver/server
class McastRcvr
{
public:
    virtual ~McastRcvr() {}
    /**
     * Handles reception of product information.
     * @param[in] prodInfo  The product information
     */
    virtual void recvMcast(const ProdInfo prodInfo) =0;
    /**
     * Handles reception of a data segment.
     * @param[in] dataSeg  The data segment
     */
    virtual void recvMcast(const DataSeg dataSeg) =0;
};

class Peer;

} // namespace

namespace std {
    /// Class function for hashing a notice
    template<>
    struct hash<hycast::Notice> {
        /// Returns the hash value of a notice
        size_t operator()(const hycast::Notice& datumId) const noexcept {
            return datumId.hash();
        }
    };
}

#endif /* MAIN_PROTO_HYCASTPROTO_H_ */
