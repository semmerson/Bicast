/**
 * This file defines information on a peer-to-peer server
 *
 * @file:   P2pSrvrInfo.cpp
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
#include "P2pSrvrInfo.h"
#include "Xprt.h"

namespace bicast {

P2pSrvrInfo::P2pSrvrInfo(
        const SockAddr      srvrAddr,
        const int           numAvail,
        const int           tier,
        const SysTimePoint& valid)
    : valid(valid)
    , srvrAddr(srvrAddr)
    , tier(static_cast<Tier>(tier))
    , numAvail(static_cast<NumAvail>(numAvail))
{
    LOG_ASSERT(tier >= -1 && tier <= std::numeric_limits<Tier>::max());
    LOG_ASSERT(numAvail >= -1 && numAvail <= std::numeric_limits<NumAvail>::max());
}

String P2pSrvrInfo::to_string() const {
    return "{addr=" + srvrAddr.to_string() + ", tier=" + std::to_string(tier) +
            ", numAvail=" + std::to_string(numAvail) + ", valid=" + std::to_string(valid) + "}";
}

bool P2pSrvrInfo::write(Xprt& xprt) const {
    //LOG_TRACE("Writing server address");
    if (!srvrAddr.write(xprt))
        return false;
    //LOG_TRACE("Writing tier");
    if (!xprt.write(tier))
        return false;
    //LOG_TRACE("Writing available number");
    if (!xprt.write(numAvail))
        return false;
    //LOG_TRACE("Writing valid time");
    if (!xprt.write(valid))
        return false;
    return true;
}

bool P2pSrvrInfo::read(Xprt& xprt) {
    return srvrAddr.read(xprt) &&
            xprt.read(tier) &&
            xprt.read(numAvail) &&
            xprt.read(valid);
}

} // namespace
