/**
 * A data-segment. Data-segments are fixed-size, contiguous pieces of a
 * data-product (the last segment, however, will usually be smaller).
 *
 *        File: Segment.h
 *  Created on: Oct 21, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
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
