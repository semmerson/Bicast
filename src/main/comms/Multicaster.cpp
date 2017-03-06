/**
 * This file implements a handle class for a receiver of multicast objects.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastRcvr.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"
#include "Multicaster.h"

namespace hycast {

/**
 * Implementation of a receiver of objects serialized in UDP packets.
 */
class Multicaster::Impl
{
    typedef enum {
        PROD_INFO,
        CHUNK
    } ObjectId;

    McastUdpSock   mcastSock; // Multicast socket from which to read objects
    const unsigned version;   // Protocol version
    MsgRcvr*       msgRcvr;   // Receiver of multicast objects

public:
    /**
     * Constructs.
     * @param[in] mcastSock  Multicast socket
     * @param[in] version    Protocol version
     * @param[in] msgRcvr    Receiver of multicast objects or `nullptr`. If
     *                       non-null, then must exist for the duration of the
     *                       constructed instance.
     */
    Impl(   McastUdpSock&  mcastSock,
            const unsigned version,
            McastMsgRcvr*  msgRcvr)
        : mcastSock{mcastSock}
        , version{version}
        , msgRcvr{msgRcvr}
    {}

    /**
     * Runs a receiver that reads multicast objects from the UDP socket and
     * passes them to the message receiver specified at construction. Doesn't
     * return until the underlying socket is closed or an exception occurs.
     */
    void runReceiver()
    {
        try {
            int entryCancelState;
            (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryCancelState);
            for (;;) {
                uint16_t objId;
                (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
                ssize_t nbytes = mcastSock.InRecStream::recv(&objId, sizeof(objId), true);
                (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
                if (nbytes == 0)
                    break; // Socket closed
                objId = ntohs(objId);
                switch (objId) {
                    case PROD_INFO: {
                        char buf[mcastSock.getSize()];
                        mcastSock.InRecStream::recv(&buf, sizeof(buf));
                        ProdInfo prodInfo = ProdInfo::deserialize(buf+sizeof(objId),
                                sizeof(buf)-sizeof(objId), version);
                        msgRcvr->recvNotice(prodInfo);
                        break;
                    }
                    case CHUNK: {
                        /*
                         * For an unknown reason, the compiler complains if the
                         * `peer->recvData` parameter is a `LatentChunk&` and not a
                         * `LatentChunk`.  This is acceptable, however, because
                         * `LatentChunk` can be trivially copied. See
                         * `MsgRcvr::recvData()`.
                         */
                        LatentChunk chunk{mcastSock, version};
                        msgRcvr->recvData(chunk);
                        if (chunk.hasData())
                            throw std::logic_error(
                                    "Latent chunk-of-data still has data");
                        break;
                    }
                    default:
                        mcastSock.discard();
                }
            }
            (void)pthread_setcancelstate(entryCancelState, nullptr);
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Error running UDP multicast receiver"));
        }
    }

    /**
     * Multicasts information on a product.
     * @param[in] prodInfo  Product information
     * @throws std::system_error  I/O failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void send(ProdInfo& prodInfo)
    {
        alignas(4) char buf[4+prodInfo.getSerialSize(version)];
        *reinterpret_cast<uint32_t*>(&buf) = ::htonl(PROD_INFO);
        prodInfo.serialize(buf+4, sizeof(buf)-4, version);
        mcastSock.send(buf, sizeof(buf));
    }
};

Multicaster::Multicaster(
        McastUdpSock&  mcastSock,
        const unsigned version,
        McastMsgRcvr*  msgRcvr)
    : pImpl{new Multicaster::Impl(mcastSock, version, msgRcvr)}
{}

void hycast::Multicaster::runReceiver()
{
    pImpl->runReceiver();
}

void Multicaster::send(ProdInfo& prodInfo) const
{
    pImpl->send(prodInfo);
}

} // namespace
