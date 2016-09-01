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

#include "PeerId.h"

#include <cstring>
#include <cstdint>

namespace hycast {

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

PeerId::PeerId(const PeerId& that)
    : id_(new uint8_t[that.size_]),
      size_(that.size_)
{
    (void)memcpy(id_, that.id_, size_);
}

PeerId::~PeerId()
{
    delete[] id_;
}

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

size_t PeerId::hash() const
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

int PeerId::compare(const PeerId& that) const
{
    if (size_ < that.size_)
        return -1;
    if (size_ > that.size_)
        return 1;
    return memcmp(id_, that.id_, size_);
}

bool PeerId::equals(const PeerId& that) const
{
    return (size_ == that.size_) && (memcmp(id_, that.id_, size_) == 0);
}

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
