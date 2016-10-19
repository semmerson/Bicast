/**
 * This file declares a chunk of data that is pointed to.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: OutgoingChunk.h
 * @author: Steven R. Emmerson
 */

#ifndef OUTGOINGCHUNK_H_
#define OUTGOINGCHUNK_H_

#include "HycastTypes.h"
#include "Socket.h"

namespace hycast {

class OutgoingChunk {
    ProdIndex  prodIndex;
    ChunkIndex chunkIndex;
    Socket&    sock;
public:
    /**
     * Constructs from an SCTP socket whose current message is a chunk of data.
     * @param[in] sock  SCTP socket
     * @throws std::invalid_argument if the current message is invalid
     */
    OutgoingChunk(Socket& sock);
    /**
     * Returns the product index.
     * @return the product index
     */
    ProdIndex getProdIndex() const;
    /**
     * Returns the chunk of data index.
     * @return the chunk of data index
     */
    ChunkIndex getChunkIndex() const;
    /**
     * Returns the size of the chunk of data.
     * @return the size of the chunk of data
     */
    ChunkSize getSize() const;
    /**
     * Drains the chunk of data into a buffer. The SCTP message will no longer
     * exist.
     * @param[in] buf  Buffer to drain the chunk of data into
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety Safe
     */
    void drainData(void* buf);
};

} // namespace

#endif /* OUTGOINGCHUNK_H_ */