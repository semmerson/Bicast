/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerId.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines a unique identifier for a remote peer.
 */

#include "../comms/PeerId.h"

#include <cstring>
#include <cstdint>

namespace hycast {

/**
 * Constructs from a peer identifier.
 * @param[in] id    Peer identifier
 * @param[in] size  Size of `id` in bytes
 * @throws std::bad_alloc if required memory can't be allocated
 * @throws std::invalid_argument if `size == 0`
 * @exceptionsafety Strong
 */
PeerId::PeerId(
        const uint8_t* id,
        const size_t   size)
    : id_(new uint8_t[size]),
      size_(size)
{
    if (size == 0) {
        delete[] id;
        throw std::invalid_argument("Empty peer identifier");
    }
    (void)memcpy(id_, id, size);
}

/**
 * Copy constructs.
 * @param[in] that  Other instance
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
PeerId::PeerId(const PeerId& that)
    : id_(new uint8_t[that.size_]),
      size_(that.size_)
{
    (void)memcpy(id_, that.id_, size_);
}

/**
 * Destroys this instance.
 * @exceptionsafety Nothrow
 */
PeerId::~PeerId() noexcept
{
    delete[] id_;
}

/**
 * Copy assigns.
 * @param[in] rhs  Other instance
 * @return This instance
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
PeerId& PeerId::operator =(const PeerId& rhs)
{
    // Self-assignment and exception safe
    uint8_t* newId = new uint8_t[rhs.size_];
    (void)memcpy(newId, rhs.id_, rhs.size_);
    delete[] id_;
    id_ = newId;
    size_ = rhs.size_;
    return *this;
}

/**
 * Returns the hash code of this instance.
 * @return Hash code of this instance
 * @exceptionsafety Nothrow
 */
size_t PeerId::hash() const noexcept
{
    const uint8_t* bp = id_;
    size_t  code = 0;
    for (size_t ielt = 0; ielt < size_/sizeof(size_t); ++ielt) {
       code ^= *(const size_t*)bp;
       bp += sizeof(size_t);
    }
    if (bp < id_ + size_) {
        size_t val = *bp++;
        while (bp < id_ + size_) {
            val <<= 8;
            val += *bp++;
        }
        code ^= val;
    }
    return code;
}

/**
 * Compares this instance with another.
 * @param[in] that  Other instance
 * @retval <0 This instance is less than the other
 * @retval  0 This instance is equal to the other
 * @retval >0 This instance is greater than the other
 * @exceptionsafety Nothrow
 */
int PeerId::compare(const PeerId& that) const noexcept
{
    if (size_ < that.size_)
        return -1;
    if (size_ > that.size_)
        return 1;
    return memcmp(id_, that.id_, size_);
}

/**
 * Indicates if this instance is equal to another.
 * @param[in] that  Other instance
 * @retval `true`  This instance equals the other
 * @retval `false` This instance doesn't equal the other
 * @exceptionsafety Nothrow
 */
bool PeerId::equals(const PeerId& that) const noexcept
{
    return (size_ == that.size_) && (memcmp(id_, that.id_, size_) == 0);
}

/**
 * Returns the string representation of a peer identifier.
 * @return String representation of a peer identifier
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
std::string PeerId::to_string() const
{
    char   buf[2+2*size_];
    char*  cp = buf + 2;
    (void)memcpy(buf, "0x", 2);
    for (const uint8_t* bp = id_; bp < id_ + size_; ++bp)
        cp += sprintf(cp, "%02x", *bp);
    return std::string(buf, sizeof(buf));
}

} // namespace
