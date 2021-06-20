/**
 * This file  
 *
 *  @file:  
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

#include "HycastProto.h"

#include <cstring>
#include <ctime>

namespace hycast {

std::string DataSegId::to_string(const bool withName) const
{
    String string;
    if (withName)
        string += "DataSegId";
    return string + "{prodIndex=" + std::to_string(prodIndex) +
            ", offset=" + std::to_string(offset) + "}";
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
    Timestamp created; ///< When product was created

public:
    Impl() =default;

    Impl(    const ProdIndex   index,
             const std::string name,
             const ProdSize    size,
             const Timestamp   created)
        : index{index}
        , name(name)
        , size{size}
        , created(created)
    {}

    Impl(    const ProdIndex   index,
             const std::string name,
             const ProdSize    size)
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

    bool operator==(const Impl& rhs) const {
        return index == rhs.index &&
               name == rhs.name &&
               size == rhs.size &&
               created == rhs.created;
    }

    String to_string(const bool withName) const
    {
        String string;
        if (withName)
            string += "ProdInfo";
        return string + "{index=" + index.to_string() + ", name=\"" + name +
                "\", size=" + std::to_string(size) + ", created=" +
                created.to_string() + "}";
    }
};

ProdInfo::ProdInfo() =default;

ProdInfo::ProdInfo(const ProdIndex   index,
                   const std::string name,
                   const ProdSize    size,
                   const Timestamp   created)
    : pImpl{std::make_shared<Impl>(index, name, size, created)}
{}

ProdInfo::ProdInfo(const ProdIndex   index,
                   const std::string name,
                   const ProdSize    size)
    : pImpl{std::make_shared<Impl>(index, name, size)}
{}

const ProdIndex& ProdInfo::getProdIndex() const {
    return pImpl->index;
}
const String&    ProdInfo::getName() const {
    return pImpl->name;
}
const ProdSize&  ProdInfo::getProdSize() const {
    return pImpl->size;
}
const Timestamp& ProdInfo::getTimestamp() const {
    return pImpl->created;
}

bool ProdInfo::operator==(const ProdInfo& rhs) const {
    return pImpl->operator==(*rhs.pImpl);
}

String ProdInfo::to_string(const bool withName) const {
    return pImpl->to_string(withName);
}

} // namespace
