/**
 * This file defines a class for notices and requests sent to a remote peer.
 *
 * @file:   Notice.cpp
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

#include "config.h"

#include "logging.h"
#include "Notice.h"

#include "BicastProto.h"

namespace bicast {

Notice::Notice() noexcept
    : id(Id::UNSET)
{}

Notice::Notice(const Id id)
    : id(id)
{}

Notice::Notice(const P2pSrvrInfo& srvrInfo) noexcept
    : id(Id::P2P_SRVR_INFO)
    , srvrInfo(srvrInfo)
{}

Notice::Notice(const Tracker tracker) noexcept
    : id(Id::P2P_SRVR_INFOS)
    , tracker(tracker)
{}

Notice::Notice(const ProdId prodId) noexcept
    : id(Id::PROD_ID)
    , prodId(prodId)
{}

Notice::Notice(const DataSegId dataSegId) noexcept
    : id(Id::DATA_SEG_ID)
    , dataSegId(dataSegId)
{}

Notice::Notice(const Notice& that) noexcept
{
    id = that.id;
    switch (id) {
        case Id::P2P_SRVR_INFO:  new (&srvrInfo)  auto(that.srvrInfo);  break;
        case Id::P2P_SRVR_INFOS: new (&tracker)   auto(that.tracker);   break;
        case Id::PROD_ID:        new (&prodId)    auto(that.prodId);    break;
        case Id::DATA_SEG_ID:    new (&dataSegId) auto(that.dataSegId); break;
        default:                                                        break;
    }
}

Notice Notice::createHeartbeat() noexcept
{
    return Notice(Id::HEARTBEAT);
}

Notice::~Notice() noexcept {
    switch (id) {
        case Id::P2P_SRVR_INFO:  srvrInfo.~P2pSrvrInfo(); break;
        case Id::P2P_SRVR_INFOS: tracker.~Tracker();      break;
        case Id::PROD_ID:        prodId.~ProdId();        break;
        case Id::DATA_SEG_ID:    dataSegId.~DataSegId();  break;
        default:                                          break;
    }
}

Notice& Notice::operator=(const Notice& rhs) noexcept {
    //LOG_TRACE("this=" + to_string() + ", rhs=" + rhs.to_string());
    this->~Notice();
    id = rhs.id;
    switch (id) {
        case Id::P2P_SRVR_INFO:  new (&srvrInfo)  auto(rhs.srvrInfo);  break;
        case Id::P2P_SRVR_INFOS: new (&tracker)   auto(rhs.tracker);   break;
        case Id::PROD_ID:        new (&prodId)    auto(rhs.prodId);    break;
        case Id::DATA_SEG_ID:    new (&dataSegId) auto(rhs.dataSegId); break;
        default:                                                       break;
    }
    return *this;
}

Notice::Id Notice::getType() const noexcept {
    return id;
}

bool Notice::equals(const ProdId prodId) const noexcept {
    return id == Id::PROD_ID && this->prodId == prodId;
}

bool Notice::equals(const DataSegId segId) const noexcept {
    return id == Id::DATA_SEG_ID && dataSegId == segId;
}

Notice::operator bool() const noexcept {
    return id != Id::UNSET;
}

String Notice::to_string() const {
    switch (id) {
        case Id::P2P_SRVR_INFO:  return srvrInfo.to_string();
        case Id::P2P_SRVR_INFOS: return tracker.to_string();
        case Id::PROD_ID:        return prodId.to_string();
        case Id::DATA_SEG_ID:    return dataSegId.to_string();
        case Id::HEARTBEAT:      return "<heartbeat>";
        default:                 return "<unset>";
    }
}

size_t Notice::hash() const noexcept {
    switch (id) {
        case Id::PROD_ID:     return prodId.hash();
        case Id::DATA_SEG_ID: return dataSegId.hash();
        default:              return 0;
    }
}

bool Notice::operator==(const Notice& rhs) const noexcept {
    if (id != rhs.id)     return false;
    switch (id) {
        case Id::UNSET:       return true;
        case Id::PROD_ID:     return prodId == rhs.prodId;
        case Id::DATA_SEG_ID: return dataSegId == rhs.dataSegId;
        case Id::HEARTBEAT:   return true;
        default:              return false;
    }
}

} // namespace
