/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerId.h
 * @author: Steven R. Emmerson
 *
 * This file declares a unique identifier for a remote peer.
 */

#ifndef PEERID_H_
#define PEERID_H_

#include "InetSockAddr.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hycast {

class PeerId final {
    uint8_t* id_;
    size_t   size_;
public:
    PeerId(
            const unsigned char* id,
            const size_t         size);
    PeerId(const PeerId& that);
    ~PeerId();
    PeerId& operator=(const PeerId& rhs);
    bool operator==(const PeerId& that) const {return equals(that);}
    size_t hash() const;
    int compare(const PeerId& that) const;
    bool equals(const PeerId& that) const;
    std::string to_string() const;
};

} // namespace

#endif /* PEERID_H_ */
