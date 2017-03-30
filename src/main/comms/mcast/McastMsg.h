/**
 * This file defines the union of messages that are multicast.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastMsg.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MCAST_MCASTMSG_H_
#define MAIN_MCAST_MCASTMSG_H_

#include "ProdInfo.h"
#include "Chunk.h"
#include "Serializable.h"

#include <memory>

namespace hycast {

class McastMsg final : public Serializable<McastMsg>
{
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    McastMsg(const ProdInfo& info);

    McastMsg(const ActualChunk& chunk);

    size_t getSerialSize(unsigned version) const noexcept;

    void serialize(
            Encoder&       encoder,
            const unsigned version) const;

    static McastMsg deserialize(
            Decoder&        decoder,
            const unsigned  version);

    bool isProdInfo() const noexcept;

    ProdInfo getProdInfo() const;

    bool isLatentChunk() const noexcept;

    LatentChunk getLatentChunk() const;
};

} // namespace

#endif /* MAIN_MCAST_MCASTMSG_H_ */
