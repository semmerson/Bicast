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

#include "Multicaster.h"

namespace hycast {

/**
 * Implementation of a receiver of objects serialized in UDP packets.
 */
class Multicaster::Impl
{
    enum {
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
            MsgRcvr*       msgRcvr)
        : mcastSock{mcastSock}
        , version{version}
        , msgRcvr{msgRcvr}
    {}

    /**
     * Runs a receiver that reads multicast objects and passes them to the
     * message receiver specified at construction. Doesn't return until the
     * underlying socket is closed or an exception occurs.
     */
    void runReceiver()
    {
        int entryCancelState;
        (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryCancelState);
        for (;;) {
            uint32_t buf;
            (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
            ssize_t nbytes = mcastSock.peek(&buf, sizeof(buf));
            (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
            if (nbytes == 0)
                break; // Socket closed
            ObjectId objId = ntohl(buf);
            switch (objId) {
                case PROD_INFO:
                    char buf[size];
                    mcastSock.recv(&buf, sizeof(buf));
                    ProdInfo prodInfo = ProdInfo::deserialize(buf,
                            sizeof(buf)-4, version);
                    msgRcvr.recvNotice(prodInfo);
                    break;
                case CHUNK: {
                    /*
                     * For an unknown reason, the compiler complains if the
                     * `peer->recvData` parameter is a `LatentChunk&` and not a
                     * `LatentChunk`.  This is acceptable, however, because
                     * `LatentChunk` can be trivially copied. See
                     * `MsgRcvr::recvData()`.
                     */
                    LatentChunk chunk = LatentChunk(buf+4, sizeof(buf)-4);
                    msgRcvr.recvData(chunk);
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

    /**
     * Multicasts information on a product.
     * @param[in] prodInfo  Product information
     * @throws std::system_error  I/O failure
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Safe
     */
    void send(ProdInfo& prodInfo) const
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
        MsgRcvr*       msgRcvr)
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
