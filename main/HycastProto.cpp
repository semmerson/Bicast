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
#include "HycastProto.h"
#include "Xprt.h"

#include <cstring>
#include <ctime>

namespace hycast {

const PduId PduId::UNSET(0);
const PduId PduId::PUB_PATH_NOTICE(1);
const PduId PduId::PROD_INFO_NOTICE(2);
const PduId PduId::DATA_SEG_NOTICE(3);
const PduId PduId::PROD_INFO_REQUEST(4);
const PduId PduId::DATA_SEG_REQUEST(5);
const PduId PduId::PROD_INFO(6);
const PduId PduId::DATA_SEG(7);

PduId::PduId(Type value)
    : value(value)
{
    if (value > 7)
        throw INVALID_ARGUMENT("value=" + to_string());
}

std::string DataSegId::to_string(const bool withName) const
{
    String string;
    if (withName)
        string += "DataSegId";
    return string + "{prodIndex=" + prodIndex.to_string() +
            ", offset=" + std::to_string(offset) + "}";
}

String NoteReq::to_string() const {
    return (id == Id::PROD_INDEX)
            ? prodIndex.to_string()
            : (id == Id::DATA_SEG_ID)
                ? dataSegId.to_string()
                : "<unset>";
}

size_t NoteReq::hash() const noexcept {
    return (id == Id::PROD_INDEX)
            ? prodIndex.hash()
            : (id == Id::DATA_SEG_ID)
                ? dataSegId.hash()
                : 0;
}

bool NoteReq::operator==(const NoteReq& rhs) const noexcept {
    if (id != rhs.id)    return false;
    if (id == Id::UNSET) return true;
    return (id == Id::PROD_INDEX)
            ? prodIndex == rhs.prodIndex
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

    ProdIndex index;   ///< Product index
    String    name;    ///< Name of product
    ProdSize  size;    ///< Size of product in bytes

public:
    Impl() =default;

    Impl(    const ProdIndex   index,
             const std::string name,
             const ProdSize    size)
        : index{index}
        , name(name)
        , size{size}
    {}

    bool operator==(const Impl& rhs) const {
        return index == rhs.index &&
               name == rhs.name &&
               size == rhs.size;
    }

    String to_string(const bool withName) const
    {
        String string;
        if (withName)
            string += "ProdInfo";
        return string + "{index=" + index.to_string() + ", name=\"" + name +
                "\", size=" + std::to_string(size) + "}";
    }

    bool write(Xprt& xprt) const {
        LOG_NOTE("Writing product information to %s", xprt.to_string().data());
        auto success = index.write(xprt);
        if (success) {
            LOG_NOTE("Writing product name to %s", xprt.to_string().data());
            success = xprt.write(name);
            if (success) {
                LOG_NOTE("Writing product size to %s", xprt.to_string().data());
                success = xprt.write(size);
            }
        }
        return success;
    }

    bool read(Xprt& xprt) {
        LOG_NOTE("Reading product information from %s", xprt.to_string().data());
        auto success = index.read(xprt);
        if (success) {
            LOG_NOTE("Reading product name from %s", xprt.to_string().data());
            success = xprt.read(name);
            if (success) {
                LOG_NOTE("Reading product size from %s", xprt.to_string().data());
                success = xprt.read(size);
            }
        }
        return success;
    }
};

ProdInfo::ProdInfo() =default;

ProdInfo::ProdInfo(const ProdIndex   index,
                   const std::string name,
                   const ProdSize    size)
    : pImpl{std::make_shared<Impl>(index, name, size)}
{}

ProdInfo::operator bool() const {
    return static_cast<bool>(pImpl);
}

const ProdIndex& ProdInfo::getIndex() const {
    return pImpl->index;
}
const String&    ProdInfo::getName() const {
    return pImpl->name;
}
const ProdSize&  ProdInfo::getSize() const {
    return pImpl->size;
}

bool ProdInfo::operator==(const ProdInfo& rhs) const {
    return pImpl->operator==(*rhs.pImpl);
}

String ProdInfo::to_string(const bool withName) const {
    return pImpl->to_string(withName);
}

bool ProdInfo::write(Xprt& xprt) const {
    return pImpl->write(xprt);
}

bool ProdInfo::read(Xprt& xprt) {
    if (!pImpl)
        pImpl = std::make_shared<Impl>();
    return pImpl->read(xprt);
}

} // namespace

namespace std {
    string to_string(const hycast::ProdInfo& prodInfo) {
        return prodInfo.to_string();
    }
}
