/**
 * Supports the remote procedure calls of this package
 *
 *   @file: Codec.h
 * @author: Steven R. Emmerson
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

#ifndef MAIN_RPC_RPC_H_
#define MAIN_RPC_RPC_H_

#include <hycast.h>
#include <main/inet/Socket.h>
#include <memory>

namespace hycast {

/**
 * Supports the remote procedure calls of this package
 */
class Codec
{
protected:
    class Impl;

    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs from an implementation.
     *
     * @param[in] impl  The implementation
     */
    Codec(Impl* impl);

public:
    /**
     * Default Constructs.
     */
    Codec();

    /**
     * Constructs from a socket.
     *
     * @param[in] sock  Socket
     */
    Codec(TcpSock& sock);

    /**
     * Move constructs from a socket.
     *
     * @param[in] sock  Socket
     */
    Codec(TcpSock&& sock);

    virtual void encode(const MemChunk& chunk) const =0;
};

class StreamCodec final : public Codec
{
public:
    class Impl;

    StreamCodec();

    StreamCodec(TcpSock& sock);

    StreamCodec(TcpSock&& sock);

    void encode(const in_port_t port) const;

    void encode(const ChunkId& chunkId) const;

    void encode(const MemChunk& chunk) const override;

    void encode(const void* data, const size_t nbytes) const;

    void decode(in_port_t& port) const;

    void decode(ChunkId& chunkId) const;

    void decode(TcpChunk& chunk) const;

    size_t decode(void* data, const size_t nbytes) const;
};

class RecordCodec final : public Codec
{
public:
    class Impl;

    RecordCodec();

    RecordCodec(UdpSock& sock);

    RecordCodec(UdpSock&& sock);

    void encode(const MemChunk& chunk) const override;

    void decode(UdpChunk& chunk) const;
};

} // namespace

#endif /* MAIN_RPC_RPC_H_ */
