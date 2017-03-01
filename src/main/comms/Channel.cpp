/**
 * This file implements a Peer's type-specific I/O channel.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Channel.cpp
 * @author: Steven R. Emmerson
 */

#include "Channel.h"
#include "Codec.h"

// The following are for template instantiations
#include "Chunk.h"
#include "ChunkInfo.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include "VersionMsg.h"

#include <memory>

namespace hycast {

class ImplBase
{
protected:
    class Enc : public Encoder
    {
        SctpSock       sock;
        const unsigned streamId;
    protected:
        void write(
                const struct iovec* iov,
                const int           iovcnt)
        {
            sock.sendv(streamId, iov, iovcnt);
        }
    public:
        using Encoder::write;
        Enc(    SctpSock& sock,
                unsigned  streamId)
            : Encoder{UINT16_MAX}
            , sock{sock}
            , streamId{streamId}
        {}
    };

    class Dec : public Decoder
    {
        SctpSock sock;
    protected:
        size_t getSize()
        {
            return sock.getSize();
        }
        size_t read(
                const struct iovec* iov,
                const int           iovcnt,
                const bool          peek = false)
        {
            return sock.recvv(iov, iovcnt, peek);
        }
    public:
        //using Decoder::read;
        Dec(SctpSock& sock)
            : Decoder{UINT16_MAX}
            , sock{sock}
        {}
        size_t read(size_t nbytes = 0)
        {
            return Decoder::read(nbytes);
        }
        void discard()
        {
            sock.discard();
        }
        bool hasRecord()
        {
            return sock.hasMessage();
        }
    };

    SctpSock       sock;
    const unsigned streamId;
    const unsigned version;
    Enc            encoder;
    Dec            decoder;

    ImplBase(
            SctpSock&      sock,
            const unsigned streamId,
            const unsigned version)
        : sock{sock}
        , streamId{streamId}
        , version{version}
        , encoder{sock, streamId}
        , decoder{sock}
    {}

public:
    SctpSock& getSocket()
    {
        return sock;
    }

    unsigned getStreamId() const
    {
        return streamId;
    }

    size_t getSize()
    {
        return sock.getSize();
    }

    bool hasRecord()
    {
        return sock.hasMessage();
    }

    size_t read(size_t nbytes = 0)
    {
        return decoder.read(nbytes);
    }

    void discard()
    {
        sock.discard();
    }
};

/**
 * @tparam S  Type of sent object
 * @tparam R  Type of received object
 */
template<class S, class R>
class Channel<S,R>::Impl final : public ImplBase
{
public:
    Impl(   SctpSock&      sock,
            const unsigned streamId,
            const unsigned version)
        : ImplBase{sock, streamId, version}
    {}

    void send(S& obj)
    {
        obj.serialize(encoder, version);
        encoder.write();
    }

    R recv();
};

template<class S, class R>
R Channel<S,R>::Impl::recv()
{
    decoder.read();
    R obj = R::deserialize(decoder, version);
    decoder.discard();
    return obj;
}

template<>
LatentChunk Channel<ActualChunk,LatentChunk>::Impl::recv()
{
    ImplBase::read(ChunkInfo::getStaticSerialSize(version));
    auto obj = LatentChunk::deserialize(decoder, version);
    decoder.discard();
    return obj;
}

template<class S, class R>
Channel<S,R>::Channel(
        SctpSock&      sock,
        const unsigned streamId,
        const unsigned version)
    : pImpl{new Impl(sock, streamId, version)}
{}

template<class S, class R>
SctpSock& Channel<S,R>::getSocket() const
{
    return pImpl->getSocket();
}

template<class S, class R>
size_t Channel<S,R>::getSize() const
{
    return pImpl->getSize();
}

template<class S, class R>
void Channel<S,R>::send(S& obj) const
{
    pImpl->send(obj);
}

template<class S, class R>
R Channel<S,R>::recv() const
{
    return pImpl->recv();
}

template class Channel<VersionMsg>;
template class Channel<ProdIndex>;
template class Channel<ProdInfo>;
template class Channel<ChunkInfo>;
template class Channel<ActualChunk, LatentChunk>;

} // namespace
