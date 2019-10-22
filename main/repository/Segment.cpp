/**
 * A data-segment. Data-segments are fixed-size, contiguous pieces of a
 * data-product (the last segment, however, will usually be smaller).
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Segment.cpp
 *  Created on: Oct 21, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "Segment.h"

namespace hycast {

class Segment::Impl {
    Chunk chunk;

public:
    Impl(   ChunkSize canonSize,
            Chunk&    chunk)
        : chunk{chunk}
    {
        if (chunk.get)
    }

    void write(void* bytes);
};

Segment::Segment(
        ChunkSize canonSize,
        Chunk&    chunk)
    : pImpl{new Impl(canonSize, chunk)}
{}

} // namespace
