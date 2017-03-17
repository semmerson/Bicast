/**
 * This file implements an unknown product -- one that doesn't have any product
 * information. It's used to accumulate chunks-of-data for a product until the
 * product's information becomes available.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: UnknownProd.cpp
 * @author: Steven R. Emmerson
 */

#include "config.h"

#include "UnknownProd.h"

#include <cstring>

namespace hycast {

UnknownProd::Chunk::Chunk()
    : data{}
    , size{0}
{}

bool UnknownProd::Chunk::setIfNot(LatentChunk& latentChunk)
{
    void* buf = data.get();
    if (buf != nullptr)
        return false;
    size = latentChunk.getSize();
    buf = new char[size];
    data.reset(buf);
    latentChunk.drainData(buf, size);
    return true;
}

void* UnknownProd::Chunk::getData()
{
    return data.get();
}

ChunkSize UnknownProd::Chunk::getSize()
{
    return size;
}

UnknownProd::UnknownProd()
    : chunks{}
{}

bool UnknownProd::add(LatentChunk& latentChunk)
{
    auto chunkIndex = latentChunk.getChunkIndex();
    return chunks[chunkIndex].setIfNot(latentChunk);
}

Product UnknownProd::makeProduct(const ProdInfo& prodInfo)
{
    Product   product(prodInfo);
    ProdIndex prodIndex(prodInfo.getIndex());
    for (auto pair : chunks) {
        ChunkIndex  chunkIndex = pair.first;
        ChunkInfo   chunkInfo = prodInfo.makeChunkInfo(chunkIndex);
        Chunk       chunk{pair.second};
        ActualChunk actualChunk(chunkInfo, chunk.getData());
        product.add(actualChunk);
    }
    chunks.clear();
    return product;
}

} // namespace
