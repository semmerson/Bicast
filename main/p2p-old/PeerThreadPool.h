/**
 * Pool of threads for executing Peers.
 *
 *        File: PeerThreadPool.h
 *  Created on: Aug 8, 2019
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

#ifndef MAIN_PEER_PEERTHREADPOOL_CPP_
#define MAIN_PEER_PEERTHREADPOOL_CPP_

#include "Peer.h"

#include <memory>

namespace hycast {

class PeerThreadPool
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     *
     * @param[in] numThreads             Number of threads in the pool
     */
    explicit PeerThreadPool(const size_t numThreads);

    PeerThreadPool(const PeerThreadPool& pool) =default;

    PeerThreadPool& operator=(const PeerThreadPool& rhs) =default;

    /**
     * Executes a peer.
     *
     * @param[in] peer     Peer to be executed
     * @retval    `true`   Success
     * @retval    `false`  Failure. All threads are busy.
     */
    bool execute(Peer peer);
};

} // namespace

#endif /* MAIN_PEER_PEERTHREADPOOL_CPP_ */
