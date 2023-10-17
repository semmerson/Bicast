/**
 * This file declares information on a peer-to-peer server
 *
 * @file:   P2pSrvrInfo.h
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

#ifndef MAIN_P2P_P2PSRVRINFO_H_
#define MAIN_P2P_P2PSRVRINFO_H_

#include "BicastProto.h"
#include "CommonTypes.h"
#include "SockAddr.h"
#include "XprtAble.h"

#include <cstdint>

namespace bicast {

/// Information on a peer-to-peer server
struct P2pSrvrInfo final : public XprtAble
{
    using NumAvail = int16_t; ///< Number of peer connections

    SysTimePoint valid;    ///< When this information was valid
    SockAddr     srvrAddr; ///< Socket address of the server
    Tier         tier;     ///< Minimum number of hops to the publisher (publisher is 0)
    NumAvail     numAvail; ///< Number of unused, server-side connections available

    /**
     * Constructs.
     * @param[in] srvrAddr     Socket address of the P2P-server
     * @param[in] tier         Minimum number of hops in the P2P network to the publisher
     * @param[in] numAvail     Number of available server-side connections
     * @param[in] valid        Time when this information was valid
     * @throw InvalidArgument  `tier` or `numAvail` can't be represented
     */
    explicit P2pSrvrInfo(
            const SockAddr      srvrAddr = SockAddr(),
            const int           numAvail = -1,
            const int           tier = -1,
            const SysTimePoint& valid = SysClock::now());

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     */
    inline operator bool() const {
        return static_cast<bool>(srvrAddr);
    }

    /**
     * Indicates if the tier number and number of available server-side connections are valid.
     * @retval true   They are valid
     * @retval false  The are not valid
     */
    inline bool validMetrics() const noexcept {
        return tier >= 0 && numAvail >= 0;
    }

    /**
     * Indicates if a tier number is valid.
     * @param[in] tier  The tier number in question
     * @retval true     The tier number is valid
     * @retval false    The tier number is not valid
     */
    inline static bool validTier(const Tier tier) {
        return tier >= 0;
    }

    /**
     * Indicates if the tier number in this instance is valid.
     * @retval true   The tier number is valid
     * @retval false  The tier number is not valid
     */
    inline bool validTier() const noexcept {
        return validTier(tier);
    }

    /**
     * Returns a string representation of this instance.
     * @return String representation of this instance
     */
    String to_string() const;

    /**
     * Returns the tier number that a remote peer would have due to the P2P-server referenced by
     * this instance.
     * @return Local tier number for a remote peer. Might be invalid.
     */
    inline Tier getRmtTier() const noexcept {
        return validTier() ? tier + 1 : -1;
    }

    /**
     * Writes itself to a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(Xprt& xprt) const override;

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool read(Xprt& xprt) override;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs  The other instance
     * @retval true    This instance is equal to the other
     * @retval false   This instance is not equal to the other
     */
    bool operator==(const P2pSrvrInfo& rhs) {
        return valid == valid ||
                (srvrAddr == rhs.srvrAddr && tier == rhs.tier && numAvail == rhs.numAvail);
    }
};

} // namespace

#endif /* MAIN_P2P_P2PSRVRINFO_H_ */
