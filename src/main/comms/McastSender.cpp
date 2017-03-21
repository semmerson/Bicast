/**
 * This file implements a multicast sender of data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastSender.cpp
 * @author: Steven R. Emmerson
 */

#include "config.h"

#include "Codec.h"
#include "McastSender.h"
#include "UdpSock.h"

namespace hycast {

class McastSender::Impl final
{
    class Enc final : public Encoder
    {
        OutUdpSock sock;

    protected:
        virtual void write(
                const struct iovec* iov,
                const int           iovcnt)
        {
            sock.send(iov, iovcnt);
        }

    public:
        Enc(const InetSockAddr& mcastAddr)
            : Encoder(UdpSock::maxPayload)
            , sock(mcastAddr)
        {}
    };

    Enc            encoder;
    const unsigned version;

    /**
     * Multicasts a product-information datagram.
     * @param[in] prodInfo  Information on the data-product
     */
    void send(const ProdInfo prodInfo)
    {
        encoder.encode(prodInfoId);
        prodInfo.serialize(encoder, version);
        encoder.flush();
    }

    /**
     * Multicasts the data of a data-product.
     * @param[in] prodInfo   Product information
     * @param[in] data       Pointer to the data-product's data
     */
    void send(
            const ProdInfo  prodInfo,
            const char*     data)
    {
        const auto prodIndex = prodInfo.getIndex();
        const auto numChunks = prodInfo.getNumChunks();
        const auto prodSize = prodInfo.getSize();
        for (ChunkIndex chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex) {
            encoder.encode(chunkId);
            const auto chunkSize = prodInfo.getChunkSize(chunkIndex);
            ChunkInfo  chunkInfo(prodIndex, prodSize, chunkIndex);
            chunkInfo.serialize(encoder, version);
            encoder.encode(data, chunkSize);
            encoder.flush();
            data += chunkSize;
        }
    }

public:
    /**
     * Constructs.
     * @param[in] mcastAddr  Socket address of the multicast group
     * @param[in] version    Protocol version
     * @throws std::system_error  `socket()` failure
     */
    Impl(   const InetSockAddr& mcastAddr,
            const unsigned      version)
        : encoder(mcastAddr)
        , version{version}
    {}

    /**
     * Multicasts a data-product.
     * @param[in] prod  Data-product to be multicasted
     * @exceptionsafety Basic guarantee
     * @threadsafety    Safe
     */
    void send(Product& prod)
    {
        // Keep consistent with McastReceiver::operator()()
        const ProdInfo prodInfo = prod.getInfo();
        send(prodInfo);
        send(prodInfo, prod.getData());
    }
};

void McastSender::send(Product& prod)
{
    pImpl->send(prod);
}

McastSender::McastSender(
        const InetSockAddr& mcastAddr,
        const unsigned      version)
    : pImpl{new Impl(mcastAddr, version)}
{}

} // namespace
