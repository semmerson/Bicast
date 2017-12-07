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

#include <Channel.h>
#include "Codec.h"

// The following are for template instantiations
#include "Chunk.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include <memory>
#include "VersionMsg.h"

namespace hycast {

class ImplBase
{
protected:
    class Enc : public Encoder
    {
        SctpSock       sock;
        const unsigned streamId;
    protected:
        /**
         * This is a cancellation point.
         * @param[in] iov     I/O vector
         * @param[in] iovcnt  Size of I/O vector
         */
        void write(
                struct iovec* iov,
                const int     iovcnt)
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
        size_t read(
                const struct iovec* iov,
                const int           iovcnt,
                const bool          peek = false)
        {
            return sock.recvv(iov, iovcnt, peek ? MSG_PEEK : 0);
        }
    public:
        Dec(SctpSock& sock)
            : Decoder{UINT16_MAX}
            , sock{sock}
        {}
        size_t fill(size_t nbytes = 0)
        {
            return Decoder::fill(nbytes);
        }
        void discard()
        {
            sock.discard();
        }
        size_t getSize()
        {
            return sock.getSize();
        }
        bool hasRecord()
        {
            return sock.hasMessage();
        }
        bool operator==(const Dec& that) const noexcept
        {
            return (this == &that) || (
                    (serialBufSize == that.serialBufSize) &&
                    (serialBuf == that.serialBuf) &&
                    (nextSerial == that.nextSerial) &&
                    (serialBufBytes == that.serialBufBytes) &&
                    (dma.iov_base == that.dma.iov_base) &&
                    (dma.iov_len == that.dma.iov_len) &&
                    (sock == that.sock)
            );
        }
        bool operator==(const Decoder& that) const noexcept
        {
            auto dec = dynamic_cast<const Dec*>(&that);
            return dec ? operator==(*dec) : false;
        }
    }; // `Dec`

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

    size_t fill(size_t nbytes = 0)
    {
        return decoder.fill(nbytes);
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

    /**
     * This is a cancellation point.
     * @param[in] obj  The serializable object to be sent
     */
    void send(const S& obj)
    {
        try {
            obj.serialize(encoder, version);
            encoder.flush();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send object over "
                    "SCTP socket " + std::to_string(sock)));
        }
    }

    R recv();
};

template<class S, class R>
R Channel<S,R>::Impl::recv()
{
    decoder.fill();
    R obj = R::deserialize(decoder, version);
    decoder.clear();
    return obj;
}

template<>
LatentChunk Channel<ActualChunk,LatentChunk>::Impl::recv()
{
    ImplBase::fill(LatentChunk::getMetadataSize(version));
    return LatentChunk::deserialize(decoder, version);
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
void Channel<S,R>::send(const S& obj) const
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
template class Channel<ChunkId>;
template class Channel<ActualChunk, LatentChunk>;

} // namespace
