/**
 * This file declares an implementation of a Peer's I/O channel.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChannelImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef CHANNELIMPL_H_
#define CHANNELIMPL_H_

#include "ChunkInfo.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include "RecStream.h"
#include "SctpSock.h"
#include "Serializable.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace hycast {

class ChannelImpl {
protected:
    class SctpStream : public InOutRecStream
    {
        SctpSock       sock;
        const unsigned streamId;

    public:
        SctpStream(
                SctpSock&          sock,
                const unsigned     streamId)
            : sock{sock}
            , streamId{streamId}
        {}

        SctpSock& getSocket()
        {
            return sock;
        }

        unsigned getStreamId()
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

        virtual void send(
                const struct iovec* const iovec,
                const int                 iovcnt);

        virtual size_t recv(
                const struct iovec* iovec,
                const int           iovcnt,
                const bool          peek = false);

        void discard()
        {
            sock.discard();
        }
    };

    SctpStream         stream;
    unsigned           version;

public:
    ChannelImpl(
            SctpSock&          sock,
            const unsigned     streamId,
            const unsigned     version)
        : stream{sock, streamId}
        , version{version}
    {}

    /**
     * Returns the associated SCTP socket.
     * @returns the associated SCTP socket
     */
    SctpSock& getSocket() {
        return stream.getSocket();
    }

    /**
     * Returns the SCTP stream ID of the current, incoming message. Waits for
     * the message if necessary.
     * @return the SCTP stream ID of the current message
     */
    unsigned getStreamId() {
        return stream.getStreamId();
    }

    /**
     * Returns the amount of available input in bytes.
     * @return The amount of available input in bytes
     */
    size_t getSize() {
        return stream.getSize();
    }
};

} // namespace

#endif /* CHANNELIMPL_H_ */
