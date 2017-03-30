/**
 * This file declares an immutable unique identifier for a remote peer.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerId.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERID_H_
#define PEERID_H_

#include "InetSockAddr.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace hycast {

class PeerId final {
    uint8_t* id_;
    size_t   size_;
public:
    /**
     * Constructs from a peer identifier.
     * @param[in] id    Peer identifier
     * @param[in] size  Size of `id` in bytes
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if `size == 0`
     * @exceptionsafety Strong
     */
    PeerId(
            const unsigned char* id,
            const size_t         size);
    /**
     * Copy constructs.
     * @param[in] that  Other instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    PeerId(const PeerId& that);
    /**
     * Destroys this instance.
     * @exceptionsafety Nothrow
     */
    ~PeerId() noexcept;
    /**
     * Copy assigns.
     * @param[in] rhs  Other instance
     * @return This instance
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    PeerId& operator=(const PeerId& rhs);
    /**
     * Indicates if this instance equals another.
     * @param[in] that  Other instance
     * @retval `true`  This instance equals the other
     * @retval `false` This instance doesn't equal the other
     * @exceptionsafety Nothrow
     */
    bool operator==(const PeerId& that) const noexcept {return equals(that);}
    /**
     * Returns the hash code of this instance.
     * @return Hash code of this instance
     * @exceptionsafety Nothrow
     */
    size_t hash() const noexcept;
    /**
     * Compares this instance with another.
     * @param[in] that  Other instance
     * @retval <0 This instance is less than the other
     * @retval  0 This instance is equal to the other
     * @retval >0 This instance is greater than the other
     * @exceptionsafety Nothrow
     */
    int compare(const PeerId& that) const noexcept;
    /**
     * Indicates if this instance is equal to another.
     * @param[in] that  Other instance
     * @retval `true`  This instance equals the other
     * @retval `false` This instance doesn't equal the other
     * @exceptionsafety Nothrow
     */
    bool equals(const PeerId& that) const noexcept;
    /**
     * Returns the string representation of a peer identifier.
     * @return String representation of a peer identifier
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    std::string to_string() const;
};

} // namespace

#endif /* PEERID_H_ */
