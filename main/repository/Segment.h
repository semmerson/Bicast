/**
 * A data-segment. Data-segments are fixed-size, contiguous pieces of a
 * data-product (the last segment, however, will usually be smaller).
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Segment.h
 *  Created on: Oct 21, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_REPOSITORY_SEGMENT_H_
#define MAIN_REPOSITORY_SEGMENT_H_

#include <hycast.h>
#include <memory>
#include <cstdint>

namespace hycast {

class DataSeg
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    Segment(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in] canonSize    Canonical segment size in bytes
     * @param[in] chunk        Segment-containing chunk
     * @throw invalidArgument  Unexpected amount of data in chunk
     */
    Segment(ChunkSize canonSize,
            Chunk&    chunk);

    /**
     * Copies the segment's data into memory.
     *
     * @param[out] bytes  Destination for data. Caller is responsible for
     *                    ensuring sufficient space.
     */
    void write(void* bytes);
};

} // namespace

#endif /* MAIN_REPOSITORY_SEGMENT_H_ */
