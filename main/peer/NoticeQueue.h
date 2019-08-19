/**
 * A thread-safe queue of notices to be sent.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: NoticeQueue.h
 *  Created on: Jun 18, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_NOTICEQUEUE_H_
#define MAIN_PEER_NOTICEQUEUE_H_

#include "Chunk.h"

#include <memory>

namespace hycast {

class NoticeQueue
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    NoticeQueue(Impl* const impl);

public:
    NoticeQueue();

    bool push(const ChunkId& chunkId);

    ChunkId pop();
};

} // namespace

#endif /* MAIN_PEER_NOTICEQUEUE_H_ */
