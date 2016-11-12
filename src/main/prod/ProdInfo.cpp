/**
 * This file defines information on a product.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfo.cpp
 * @author: Steven R. Emmerson
 */

#include "ProdInfo.h"
#include "ProdInfoImpl.h"

namespace hycast {

ProdInfo::ProdInfo()
    : pImpl(new ProdInfoImpl())
{}

ProdInfo::ProdInfo(
        const std::string& name,
        const ProdIndex    index,
        const ProdSize     size,
        const ChunkSize    chunkSize)
    : pImpl(new ProdInfoImpl(name, index, size, chunkSize))
{}

ProdInfo::ProdInfo(
        const char* const buf,
        const size_t      bufLen,
        const unsigned    version)
    : pImpl(new ProdInfoImpl(buf, bufLen, version))
{}

bool ProdInfo::operator==(const ProdInfo& that) const noexcept
{
    return *pImpl.get() == *that.pImpl.get();
}

size_t ProdInfo::getSerialSize(unsigned version) const noexcept
{
    return pImpl->getSerialSize(version);
}

char* ProdInfo::serialize(
        char*          buf,
        const size_t   bufLen,
        const unsigned version) const
{
    return pImpl->serialize(buf, bufLen, version);
}

const std::string& ProdInfo::getName() const
{
    return pImpl->getName();
}

ProdIndex ProdInfo::getIndex() const
{
    return pImpl->getIndex();
}

ProdSize ProdInfo::getSize() const
{
    return pImpl->getSize();
}

ChunkSize ProdInfo::getChunkSize() const
{
    return pImpl->getChunkSize();
}

ProdInfo ProdInfo::deserialize(
        const char* const buf,
        const size_t      size,
        const unsigned    version)
{
    return ProdInfo(buf, size, version);
}

} // namespace
