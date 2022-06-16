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
#define _XOPEN_SOURCE 700
#include "config.h"

#include "error.h"
#include "HycastProto.h"
#include "Xprt.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <inttypes.h>
#include <openssl11/openssl/sha.h>

#ifndef _XOPEN_PATH_MAX
    // Not in gcc 4.8.5 for some reason
    #define _XOPEN_PATH_MAX 1024
#endif

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
        uint64_t      uint64s[SHA256_DIGEST_LENGTH/sizeof(uint64_t)]; // 4 x 8 bytes
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

String DatumId::to_string() const {
    switch (id) {
    case Id::GOOD_P2P_SRVR:
        return srvrAddr.to_string();
    case Id::GOOD_P2P_SRVRS:
        return tracker.to_string();
    case Id::PROD_INDEX:
        return prodId.to_string();
    case Id::DATA_SEG_ID:
        return dataSegId.to_string();
    default:
        return "<unset>";
    }
}

size_t DatumId::hash() const noexcept {
    return (id == Id::PROD_INDEX)
            ? prodId.hash()
            : (id == Id::DATA_SEG_ID)
                ? dataSegId.hash()
                : 0;
}

bool DatumId::operator==(const DatumId& rhs) const noexcept {
    if (id != rhs.id)    return false;
    if (id == Id::UNSET) return true;
    return (id == Id::PROD_INDEX)
            ? prodId == rhs.prodId
            : dataSegId == rhs.dataSegId;
}

std::string Timestamp::to_string(const bool withName) const
{
    const time_t time = sec;
    const auto   timeStruct = *::gmtime(&time);
    char         buf[40];
    auto         nbytes = ::strftime(buf, sizeof(buf), "%FT%T.", &timeStruct);
    ::snprintf(buf+nbytes, sizeof(buf)-nbytes, "%06luZ}",
            static_cast<unsigned long>(nsec/1000));

    String string;
    if (withName)
        string += "TimeStamp{";
    string += buf;
    if (withName)
        string += "}";

    return string;
}

/******************************************************************************/

/// Product information
class ProdInfo::Impl
{
    friend class ProdInfo;

    using NameLenType = uint16_t; ///< Type to hold length of product-name

    ProdId    prodId;  ///< Product index
    String    name;    ///< Name of product
    ProdSize  size;    ///< Size of product in bytes

public:
    Impl() =default;

    Impl(    const ProdId   index,
             const std::string name,
             const ProdSize    size)
        : prodId{index}
        , name(name)
        , size{size}
    {
        if (name.size() > _XOPEN_PATH_MAX-1)
            throw INVALID_ARGUMENT("Name is longer than " +
                    std::to_string(_XOPEN_PATH_MAX-1) + " bytes");
    }

    bool operator==(const Impl& rhs) const {
        return prodId == rhs.prodId &&
               name == rhs.name &&
               size == rhs.size;
    }

    String to_string(const bool withName) const
    {
        String string;
        if (withName)
            string += "ProdInfo";
        return string + "{prodId=" + prodId.to_string() + ", name=\"" + name +
                "\", size=" + std::to_string(size) + "}";
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
                if (nbytes > _XOPEN_PATH_MAX-1)
                    throw RUNTIME_ERROR("Name is longer than " +
                            std::to_string(_XOPEN_PATH_MAX-1) + " bytes");
                char bytes[nbytes];
                success = xprt.read(bytes, nbytes);
                if (success) {
                    name.assign(bytes, nbytes);
                    //LOG_DEBUG("Reading product size from %s", xprt.to_string().data());
                    success = xprt.read(size);
                }
            }
        }
        return success;
    }
};

ProdInfo::ProdInfo()
    : pImpl(nullptr)
{}

ProdInfo::ProdInfo(const ProdId   index,
                   const std::string name,
                   const ProdSize    size)
    : pImpl{new Impl(index, name, size)}
{}

ProdInfo::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

const ProdId& ProdInfo::getId() const {
    return pImpl->prodId;
}
const String&    ProdInfo::getName() const {
    return pImpl->name;
}
const ProdSize&  ProdInfo::getSize() const {
    return pImpl->size;
}

bool ProdInfo::operator==(const ProdInfo rhs) const {
    return pImpl->operator==(*rhs.pImpl);
}

String ProdInfo::to_string(const bool withName) const {
    return pImpl->to_string(withName);
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
