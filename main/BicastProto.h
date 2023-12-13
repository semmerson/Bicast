/**
 * This file declares the types used in the Bicast protocol.
 * 
 * @file:   BicastProto.h
 * @author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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

#ifndef MAIN_BICASTPROTO_H_
#define MAIN_BICASTPROTO_H_

#include "CommonTypes.h"
#include "error.h"
#include "XprtAble.h"

#include <cstdint>
#include <unordered_set>

namespace bicast {

using Tier     = int16_t; ///< Number of hops to the publisher

constexpr uint8_t PROTOCOL_VERSION = 1; ///< Protocol version

/// Protocol data unit (PDU) identifiers
class PduId : public XprtAble
{
public:
    using Type = uint16_t; ///< Underlying type of protocol data unit identifier

private:
    Type value;

public:
    /// Types of protocol data units
    enum Id : Type {
        UNSET,                 ///< Not set
        SRVR_INFO,             ///< Information on a P2P-server
        TRACKER,               ///< Information on P2P-servers
        SRVR_INFO_NOTICE,      ///< Information on the remote peer's P2P-server
        PROD_INFO_NOTICE,      ///< Notice about available information on a data product
        DATA_SEG_NOTICE,       ///< Notice about an available data segment
        PREVIOUSLY_RECEIVED,   ///< Prevously-received products
        PROD_INFO_REQUEST,     ///< Request for product information
        DATA_SEG_REQUEST,      ///< Request for a data segment
        PROD_INFO,             ///< Product information
        DATA_SEG,              ///< Data segment
        HEARTBEAT,             ///< Heartbeat packet used by class Xprt
        MAX_PDU_ID = HEARTBEAT ///< Maximum PDU type
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
     * Constructs from a transport.
     * @param[in] xprt         Transport from which to read the PDU ID
     * @throw EofError         Couldn't read value
     * @throw InvalidArgument  The read value is invalid
     */
    PduId(Xprt& xprt);

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
    inline bool operator==(const Id rhs) const noexcept {
        return value == rhs;
    }

    /**
     * Writes itself to a transport.
     * @param[in] xprt
     * @return    true   Success
     * @return    false  I/O closed
     */
    bool write(Xprt& xprt) const override;

    /**
     * Reads itself from a transport.
     * @param[in] xprt         The transport
     * @return    true         Success
     * @return    false        I/O closed
     * @throw InvalidArgument  The read value is invalid
     */
    bool read(Xprt& xprt) override;
};

/******************************************************************************/
// PDU payloads

using ProdSize  = uint32_t;    ///< Size of product in bytes
using SegSize   = uint16_t;    ///< Data-segment size in bytes
using SegOffset = ProdSize;    ///< Offset of data-segment in bytes

class Xprt;

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

    /// Indicates if this instance is complete (i.e., wan't default constructed)
    operator bool() const noexcept {
        return id != 0;
    }

    /**
     * Copy assigns.
     * @param[in] rhs  The other instance
     * @return         A reference to this just-assigned instance
     */
    ProdId& operator=(const ProdId& rhs) =default;

    /**
     * Move assigns.
     * @param[in] rhs  The other instance
     * @return         A reference to this just-assigned instance
     */
    ProdId& operator=(ProdId&& rhs) =default;

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
        return id;
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
    bool write(Xprt& xprt) const override;

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool read(Xprt& xprt) override;
};

} // namespace

namespace std {
    using namespace bicast;

    /// Function class for hashing a data product's identifier.
    template<>
    struct hash<ProdId> {
        /**
         * Returns the hash code of a product identifier.
         * @param prodId  The product identifier
         * @return        The hash code of the product identifier
         */
        size_t operator()(const ProdId& prodId) const noexcept {
            return prodId.hash();
        }
    };
}

namespace bicast {

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

    /**
     * Writes itself to a transport.
     * @param[in] xprt
     * @return    true   Success
     * @return    false  Lost connection
     */
    bool write(Xprt& xprt) const override;

    /**
     * Reads itself from a transport.
     * @param[in] xprt   The transport
     * @return    true   Success
     * @return    false  Lost connection
     */
    bool read(Xprt& xprt) override;
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
             const SysTimePoint  createTime = SysClock::now());

    /**
     * Constructs.
     *
     * @param[in] name        Name of product
     * @param[in] size        Size of product in bytes
     * @param[in] createTime  When the product was created
     */
    ProdInfo(const std::string&  name,
             const ProdSize      size,
             const SysTimePoint  createTime = SysClock::now());

    /**
     * Constructs.
     *
     * @param[in] prodId      Product ID
     */
    explicit ProdInfo(const ProdId prodId)
        : ProdInfo(prodId, "", 0)
    {}

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
     * Indicates if this instance is considered the same as another.
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

    /**
     * Writes itself to a transport.
     * @param[in] xprt
     * @return    true   Success
     * @return    false  I/O closed
     */
    bool write(Xprt& xprt) const override;

    /**
     * Reads itself from a transport.
     * @param[in] xprt   The transport
     * @return    true   Success
     * @return    false  I/O closed
     */
    bool read(Xprt& xprt) override;
};

/// Data segment
class DataSeg final : public XprtAble
{
public:
    class Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
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

    /**
     * Writes itself to a transport.
     * @param[in] xprt   The transport
     * @return    true   Success
     * @return    false  I/O closed
     */
    bool write(Xprt& xprt) const override;

    /**
     * Reads itself from a transport.
     * @param[in] xprt   The transport
     * @return    true   Success
     * @return    false  I/O closed
     */
    bool read(Xprt& xprt) override;
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

    /**
     * Writes itself to a transport.
     * @param[in] xprt
     * @return    true   Success
     * @return    false  I/O closed
     */
    bool write(Xprt& xprt) const override;

    /**
     * Reads itself from a transport.
     * @param[in] xprt   The transport
     * @return    true   Success
     * @return    false  I/O closed
     */
    bool read(Xprt& xprt) override;

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

} // namespace

#endif /* MAIN_BICASTPROTO_H_ */
