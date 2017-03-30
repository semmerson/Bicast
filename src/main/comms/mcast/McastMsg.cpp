/**
 * This file implements the union of messages that are multicast.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastMsg.cpp
 * @author: Steven R. Emmerson
 */

#include <mcast/McastMsg.h>
#include "config.h"

#include "error.h"

namespace  hycast {

class McastMsg::Impl final
{
    enum {
        PROD_INFO,
        CHUNK
    } type;
    const ProdInfo    prodInfo;
    const ActualChunk actualChunk;
    LatentChunk       latentChunk;

public:
    Impl(const ProdInfo& info)
        : type{PROD_INFO}
        , prodInfo{info}
        , actualChunk{}
        , latentChunk{}
    {}

    Impl(const ActualChunk& chunk)
        : type{CHUNK}
        , prodInfo{}
        , actualChunk{chunk}
        , latentChunk{}
    {}

    bool isProdInfo() const noexcept
    {
        return type == PROD_INFO;
    }

    ProdInfo getProdInfo() const
    {
        if (!isProdInfo)
            throw LogicError(__FILE__, __LINE__,
                    "Message isn't product information");
        return msg.prodInfo;
    }

    bool isLatentChunk() const noexcept
    {
        return type == PROD_INFO;
    }

    LatentChunk getLatentChunk() const
    {
        if (!isLatentChunk)
            throw LogicError(__FILE__, __LINE__,
                    "Message isn't product information");
        return msg.latentChunk;
    }
}

McastMsg::McastMsg(const ProdInfo& info)
    : pImpl{new Impl(info)}
{}

McastMsg::McastMsg(const ActualChunk& chunk)
    : pImpl{new Impl(chunk)}
{}

bool McastMsg::isProdInfo() const
{
    return pImpl->isProdInfo();
}

ProdInfo McastMsg::getProdInfo() const
{
    return pImpl->getProdInfo();
}

bool McastMsg::isLatentChunk() const
{
    return pImpl->isLatentChunk();
}

LatentChunk McastMsg::getLatentChunk() const
{
    return pImpl->getLatentChunk();
}

} // namespace
