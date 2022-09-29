/**
 * This file implements the types used in the Hycast protocol.
 *
 *  @file:  HycastProto.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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
#include "config.h"

#include "error.h"
#include "FileUtil.h"
#include "HycastProto.h"
#include "Xprt.h"

#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <time.h>
#include <inttypes.h>
#include <openssl11/openssl/sha.h>

namespace hycast {

bool FeedInfo::write(Xprt xprt) const {
    return mcastGroup.write(xprt) &&
            mcastSource.write(xprt) &&
            xprt.write(segSize);
}

bool FeedInfo::read(Xprt xprt) {
    return mcastGroup.read(xprt) &&
            mcastSource.read(xprt) &&
            xprt.read(segSize);
}

/**
 * Using a  hash value to uniquely identify a product means that the probability of two or more
 * products having the same hash value in the repository is approximately 1 - e^-(n^2/2d), where n
 * is the number of products and d is the number of possible hash values. For an 8-byte hash value
 * and one hour of the feed with the highest rate of products (NEXRAD3: ~106e3/hr as of 2022-05)
 * this is approximately 3.06e-10. See "Birthday problem" in Wikipedia for details.
 *
 * Alternatively, the probability that an incoming product will have the same hash value as an
 * existing but different product is n/d, which is approximately 5.8e-15 in the above NEXRAD3
 * case. So there's a 50% chance of a collision in approximately 93e3 years.
 *
 * Using the hash of the product name instead of a monotonically increasing unsigned integer means
 * that 1) product names should be unique; and 2) redundant publishers are more easily supported.
 */
ProdId::ProdId(const String& prodName) {
    union {
        unsigned char bytes[SHA256_DIGEST_LENGTH]; // 32 bytes
        uint64_t      uint64s[SHA256_DIGEST_LENGTH/sizeof(uint64_t)]; // 4
    } md;
    if (SHA256(reinterpret_cast<const unsigned char*>(prodName.data()), prodName.size(), md.bytes)
            == nullptr)
        throw RUNTIME_ERROR("Couldn't compute SHA-256 hash");
    id = md.uint64s[0] ^ md.uint64s[1] ^ md.uint64s[2] ^ md.uint64s[3];
}

String ProdId::to_string() const noexcept {
    char buf[sizeof(id)*2+1];
    ::sprintf(buf, "%0" PRIx64, id);
    return String(buf);
}

std::string DataSegId::to_string(const bool withName) const
{
    String string;
    if (withName)
        string += "DataSegId";
    return string + "{prodId=" + prodId.to_string() + ", offset=" + std::to_string(offset) + "}";
}

String Notice::to_string() const {
    switch (id) {
    case Id::DATA_SEG_ID:
        return dataSegId.to_string();
    case Id::PROD_INDEX:
        return prodId.to_string();
    case Id::GOOD_P2P_SRVR:
        return srvrAddr.to_string();
    case Id::BAD_P2P_SRVR:
        return srvrAddr.to_string();
    case Id::GOOD_P2P_SRVRS:
        return tracker.to_string();
    case Id::BAD_P2P_SRVRS:
        return tracker.to_string();
    default:
        return "<unset>";
    }
}

size_t Notice::hash() const noexcept {
    return (id == Id::PROD_INDEX)
            ? prodId.hash()
            : (id == Id::DATA_SEG_ID)
                ? dataSegId.hash()
                : 0;
}

bool Notice::operator==(const Notice& rhs) const noexcept {
    if (id != rhs.id)    return false;
    if (id == Id::UNSET) return true;
    return (id == Id::PROD_INDEX)
            ? prodId == rhs.prodId
            : dataSegId == rhs.dataSegId;
}

/******************************************************************************/

/// Set of product identifiers

class ProdIdSet::Impl
{
    using Set = std::unordered_set<ProdId>;

    Set prodIds;

public:
    Impl(const size_t n)
        : prodIds{n}
    {}

    String to_string() const {
        return "{size=" + std::to_string(prodIds.size()) + "}";
    }

    void subtract(const Impl& rhs) {
        for (auto iter = rhs.prodIds.begin(), end = rhs.prodIds.end(); iter != end; ++iter)
            prodIds.erase(*iter);
    }

    bool write(Xprt xprt) const {
        if (!xprt.write(static_cast<uint32_t>(prodIds.size())))
            return false;
        for (auto iter = prodIds.begin(), end = prodIds.end(); iter != end; ++iter)
            if (!iter->write(xprt))
                return false;
        return true;
    }

    bool read(Xprt xprt) {
        uint32_t size;
        if (!xprt.read(size))
            return false;
        prodIds.clear();
        prodIds.reserve(size);
        for (uint32_t i; i < size; ++i) {
            ProdId prodId;
            if (!prodId.read(xprt))
                return false;
            prodIds.insert(prodId);
        }
        return true;
    }

    size_t size() const {
        return prodIds.size();
    }

    size_t count(const ProdId prodId) const {
        return prodIds.count(prodId);
    }

    void insert(const ProdId prodId) {
        prodIds.insert(prodId);
    }

    Set::iterator begin() {
        return prodIds.begin();
    }

    Set::iterator end() {
        return prodIds.end();
    }

    void clear() {
        prodIds.clear();
    }
};

ProdIdSet::ProdIdSet(const size_t n)
    : pImpl{new Impl(n)}
{}

String ProdIdSet::to_string() const {
    return pImpl->to_string();
}

void ProdIdSet::subtract(const ProdIdSet rhs) {
    pImpl->subtract(*rhs.pImpl);
}

bool ProdIdSet::write(Xprt xprt) const {
    return pImpl->write(xprt);
}

bool ProdIdSet::read(Xprt xprt) {
    return pImpl->read(xprt);
}

size_t ProdIdSet::size() const {
    return pImpl->size();
}

size_t ProdIdSet::count(const ProdId prodId) const {
    return pImpl->count(prodId);
}

void ProdIdSet::insert(const ProdId prodId) const {
    pImpl->insert(prodId);
}

ProdIdSet::iterator ProdIdSet::begin() const {
    return pImpl->begin();
}

ProdIdSet::iterator ProdIdSet::end() const {
    return pImpl->end();
}

void ProdIdSet::clear() const {
    pImpl->clear();
}

/******************************************************************************/

/// Product information
class ProdInfo::Impl
{
    friend class ProdInfo;

    using NameLenType = uint16_t; ///< Type to hold length of product-name

    ProdId       prodId;       ///< Product ID
    String       name;         ///< Name of product
    ProdSize     size;         ///< Size of product in bytes
    SysTimePoint creationTime; ///< When product was created

public:
    Impl() =default;

    Impl(    const ProdId       prodId,
             const std::string& name,
             const ProdSize     size,
             const SysTimePoint createTime)
        : prodId(prodId)
        , name(name)
        , size{size}
        , creationTime(createTime)
    {
        if (name.size() > PATH_MAX-1)
            throw INVALID_ARGUMENT("Name is longer than " + std::to_string(PATH_MAX-1) + " bytes");
    }

    bool operator==(const Impl& rhs) const {
        /**
         * The creation-time is not compared because timestamps created by different computers
         * that should be equal can be different.
         */
        return prodId == rhs.prodId &&
               name == rhs.name &&
               size == rhs.size;
    }

    String to_string(const bool withName) const
    {
        String string;
        if (withName)
            string += "ProdInfo";

        auto      secs = SysClock::to_time_t(creationTime);
        struct tm tmStruct;
        ::gmtime_r(&secs, &tmStruct);
        char      iso8601[28]; // "YYYY-MM-DDThh:mm:ss.uuuuuuZ"
        auto nbytes = ::strftime(iso8601, sizeof(iso8601), "%FT%T", &tmStruct);
        long usecs = std::chrono::duration_cast<std::chrono::microseconds>(
                creationTime - SysClock::from_time_t(secs)).count();
        ::snprintf(iso8601+nbytes, sizeof(iso8601)-nbytes, ".%06ldZ", usecs);

        return string + "{prodId=" + prodId.to_string() + ", name=\"" + name +
                "\", size=" + std::to_string(size) + ", created=" + iso8601 + "}";
    }

    bool write(Xprt xprt) const {
        //LOG_DEBUG("Writing product information to %s", xprt.to_string().data());
        auto success = prodId.write(xprt);
        if (success) {
            //LOG_DEBUG("Writing product name to %s", xprt.to_string().data());
            success = xprt.write(static_cast<NameLenType>(name.size())) &&
                    xprt.write(name.data(), name.size());
            if (success) {
                //LOG_DEBUG("Writing product size to %s", xprt.to_string().data());
                success = xprt.write(size);
                if (success) {
                    uint64_t count = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            creationTime.time_since_epoch()).count();
                    success = xprt.write(count);
                }
            }
        }
        return success;
    }

    bool read(Xprt xprt) {
        //LOG_DEBUG("Reading product information from %s", xprt.to_string().data());
        auto success = prodId.read(xprt);
        if (success) {
            //LOG_DEBUG("Reading product name from %s", xprt.to_string().data());
            NameLenType nbytes;
            success = xprt.read(nbytes);
            if (success) {
                if (nbytes > PATH_MAX-1)
                    throw RUNTIME_ERROR(
                            "Name is longer than " + std::to_string(PATH_MAX-1) + " bytes");
                char bytes[nbytes];
                success = xprt.read(bytes, nbytes);
                if (success) {
                    name.assign(bytes, nbytes);
                    //LOG_DEBUG("Reading product size from %s", xprt.to_string().data());
                    success = xprt.read(size);
                    if (success) {
                        uint64_t count;
                        success = xprt.read(count);
                        if (success)
                            creationTime = SysTimePoint(SysClock::duration(count));
                    }
                }
            }
        }
        return success;
    }
};

ProdInfo::ProdInfo()
    : pImpl(nullptr)
{}

ProdInfo::ProdInfo(const ProdId        index,
                   const std::string&  name,
                   const ProdSize      size,
                   const SysTimePoint& createTime)
    : pImpl{new Impl(index, name, size, createTime)}
{}

ProdInfo::ProdInfo(const std::string&  name,
                   const ProdSize      size,
                   const SysTimePoint& createTime)
    : ProdInfo(ProdId{name}, name, size, createTime)
{}

ProdInfo::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

const ProdId& ProdInfo::getId() const {
    if (!pImpl)
        throw LOGIC_ERROR("Unset product information");
    return pImpl->prodId;
}
const String&    ProdInfo::getName() const {
    if (!pImpl)
        throw LOGIC_ERROR("Unset product information");
    return pImpl->name;
}
const ProdSize&  ProdInfo::getSize() const {
    if (!pImpl)
        throw LOGIC_ERROR("Unset product information");
    return pImpl->size;
}
const SysTimePoint&  ProdInfo::getCreateTime() const {
    if (!pImpl)
        throw LOGIC_ERROR("Unset product information");
    return pImpl->creationTime;
}

bool ProdInfo::operator==(const ProdInfo rhs) const {
    return pImpl->operator==(*rhs.pImpl);
}

String ProdInfo::to_string(const bool withName) const {
    return pImpl ? pImpl->to_string(withName) : "<unset>";
}

bool ProdInfo::write(Xprt xprt) const {
    return pImpl->write(xprt);
}

bool ProdInfo::read(Xprt xprt) {
    if (!pImpl)
        pImpl = std::make_shared<Impl>();
    return pImpl->read(xprt);
}



} // namespace

namespace std {
    string to_string(const hycast::ProdId prodId) {
        return prodId.to_string();
    }
    string to_string(const hycast::ProdInfo prodInfo) {
        return prodInfo.to_string();
    }
}
