/**
 * This file declares a class for notices and requests sent to a remote peer.
 *
 * @file:   Notice.h
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

#ifndef MAIN_P2P_NOTICE_H_
#define MAIN_P2P_NOTICE_H_

#include "BicastProto.h"
#include "CommonTypes.h" // String
#include "P2pSrvrInfo.h"
#include "Tracker.h"

#include <cstddef>    // size_t
#include <functional> // std::hash()

namespace bicast {

/**
 * Class for both notices and requests sent to a remote peer. It exists so that such entities can be
 * handled as a single object for the purpose of argument passing and container element.
 */
struct Notice
{
public:
    /// Identifier of the type of notice
    enum class Id {
        UNSET,          ///< Notice was default constructed
        P2P_SRVR_INFO,  ///< Information on a P2P server
        P2P_SRVR_INFOS, ///< Information on P2P servers
        PROD_ID,        ///< Product ID
        DATA_SEG_ID,    ///< Data segment ID
        HEARTBEAT       ///< Connection heartbeat
    } id; ///< Identifier of the type of notice
    union {
        P2pSrvrInfo srvrInfo;  ///< Information on a P2P server
        Tracker     tracker;   ///< Information on P2P servers
        ProdId      prodId;    ///< Product ID
        DataSegId   dataSegId; ///< Data segment ID
    };

private:
    Notice(const Id id);

public:
    /**
     * Creates a heartbeat notice.
     * @return A heartbeat notice
     */
    static Notice createHeartbeat() noexcept;

    /// Default constructs
    Notice() noexcept;

    /**
     * Constructs a notice about a P2P-server.
     * @param[in] srvrInfo  Information on the P2P-server
     */
    explicit Notice(const P2pSrvrInfo& srvrInfo) noexcept;

    /**
     * Constructs a notice about P2P-servers.
     * @param[in] tracker  The tracker to be in the notice
     */
    explicit Notice(const Tracker tracker) noexcept;

    /**
     * Constructs a notice about an available product.
     * @param[in] prodId  The product's ID
     */
    explicit Notice(const ProdId prodId) noexcept;

    /**
     * Constructs a notice about an available data segment.
     * @param[in] dataSegId The data segment's ID
     */
    explicit Notice(const DataSegId dataSegId) noexcept;

    /**
     * Copy constructs.
     * @param[in] that  The other instance
     */
    Notice(const Notice& that) noexcept;

    /// Destroys
    ~Notice() noexcept;

    /**
     * Copy assigns.
     * @param[in] rhs  The other instance
     * @return         A reference to this just-assigned instance
     */
    Notice& operator=(const Notice& rhs) noexcept;

    /**
     * Returns the type of the notice.
     * @return The type of the notice
     */
    Id getType() const noexcept;

    /**
     * Indicates if this instance is about a given product.
     * @param[in] prodId   The product's ID
     * @retval    true     This instance is about the given product
     * @retval    false    This instance is not about the given product
     */
    bool equals(const ProdId prodId) const noexcept;

    /**
     * Indicates if this instance is about a given data segment.
     * @param[in] segId    The data segment's ID
     * @retval    true     This instance is about the given data segment
     * @retval    false    This instance is not about the given data segment
     */
    bool equals(const DataSegId segId) const noexcept;

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     * @retval true     This instance is valid
     * @retval false    This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    String to_string() const;

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

} // namespace

namespace std {
    using namespace bicast;

    /// Function class for hashing a Notice
    template<>
    struct hash<Notice> {
        /// Returns the hash value of a Notice
        size_t operator()(const Notice& notice) const {
            return notice.hash();
        }
    };
}

#endif /* MAIN_P2P_NOTICE_H_ */
