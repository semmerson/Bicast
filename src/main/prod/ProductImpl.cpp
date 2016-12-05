/**
 * This file implements the implementation of a data-product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProductImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "ProductImpl.h"

#include <cstring>
#include <stdexcept>

namespace hycast {

ProductImpl::ProductImpl(const ProdInfo& prodInfo)
    : prodInfo{prodInfo}
    // `haveChunk{n}` means add `n` rather than have `n` elements
    , haveChunk(prodInfo.getNumChunks())
    , data{new char[prodInfo.getSize()]}
{}

ProductImpl::~ProductImpl()
{
    delete[] data;
}

const ProdInfo& hycast::ProductImpl::getInfo() const noexcept
{
    return prodInfo;
}

char* ProductImpl::startOf(const ChunkIndex chunkIndex) const
{
    return data + chunkIndex * prodInfo.getChunkSize();
}

bool ProductImpl::add(const ActualChunk& chunk)
{
    ChunkSize chunkSize{chunk.getSize()};
    prodInfo.vet(chunk.getInfo(), chunkSize);
    ChunkIndex chunkIndex{chunk.getInfo().getChunkIndex()};
    if (haveChunk[chunkIndex])
        return false;
    ::memcpy(startOf(chunkIndex), chunk.getData(), chunkSize);
    return haveChunk[chunkIndex] = true;
}

bool ProductImpl::add(LatentChunk& chunk)
{
    prodInfo.vet(chunk.getInfo(), chunk.getSize());
    ChunkIndex chunkIndex{chunk.getInfo().getChunkIndex()};
    if (haveChunk[chunkIndex])
        return false;
    chunk.drainData(startOf(chunkIndex));
    return haveChunk[chunkIndex] = true;
}

bool ProductImpl::isComplete() const
{
    return haveChunk.size() == prodInfo.getNumChunks();
}

} // namespace
