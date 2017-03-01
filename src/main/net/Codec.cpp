/**
 * This file implements the base functions for serializing and deserializing
 * primitive types to and from an underlying, record-oriented, I/O object.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Codec.cpp
 * @author: Steven R. Emmerson
 */

#include "Codec.h"

#include <climits>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>

namespace hycast {

Codec::Codec(const size_t maxSize)
    : maxSize{maxSize}
    , buf{new char[maxSize]}
    , next{buf}
    , nbytes{0}
{
    dma.iov_len = 0;
}

Codec::~Codec()
{
    delete[] buf;
}

inline size_t Codec::roundup(const size_t len)
{
    return ((len + alignment - 1)/alignment) * alignment;
}

void Codec::clear() noexcept
{
    next = buf;
    nbytes = 0;
    dma.iov_len = 0;
}

size_t Codec::getSerialSize(const size_t size)
{
    return roundup(size);
}

size_t Codec::getSerialSize(const uint16_t* value)
{
    return roundup(sizeof(uint16_t));
}

size_t Codec::getSerialSize(const uint32_t* value)
{
    return roundup(sizeof(uint32_t));
}

size_t Codec::getSerialSize(const std::string& string)
{
    return roundup(sizeof(uint32_t)) + roundup(string.size());
}

Encoder::Encoder(const size_t maxSize)
    : Codec{maxSize}
{}

Encoder::~Encoder()
{}

Decoder::Decoder(const size_t maxSize)
    : Codec{maxSize}
{}

Decoder::~Decoder()
{}

void Encoder::encode(uint16_t value)
{
    const size_t len = roundup(sizeof(uint16_t));
    if (nbytes + len > maxSize)
        throw std::runtime_error("Buffer-write overflow");
    *reinterpret_cast<uint16_t*>(next) = htons(value);
    nbytes += len;
    next += len;
}

void Encoder::encode(uint32_t value)
{
    const size_t len = roundup(sizeof(uint32_t));
    if (nbytes + len > maxSize)
        throw std::runtime_error("Buffer-write overflow");
    *reinterpret_cast<uint32_t*>(next) = htonl(value);
    nbytes += len;
    next += len;
}

void Decoder::decode(uint16_t& value)
{
    const size_t len = roundup(sizeof(uint16_t));
    if (nbytes < len)
        throw std::runtime_error("Buffer-read overflow");
    value = ntohs(*reinterpret_cast<uint16_t*>(next));
    nbytes -= len;
    next += len;
}

void Decoder::decode(uint32_t& value)
{
    const size_t len = roundup(sizeof(uint32_t));
    if (nbytes < len)
        throw std::runtime_error("Buffer-read overflow");
    value = ntohl(*reinterpret_cast<uint32_t*>(next));
    nbytes -= len;
    next += len;
}

void Encoder::encode(const std::string& string)
{
    const size_t strlen = string.size();
    if (strlen > UINT32_MAX)
        throw std::invalid_argument("String too long: len=" +
                std::to_string(strlen));
    encode(static_cast<uint32_t>(strlen));
    const size_t len = roundup(strlen);
    if (nbytes + len > maxSize)
        throw std::runtime_error("Buffer-write overflow");
    memcpy(next, string.data(), strlen);
    nbytes += len;
    next += len;
}

void Decoder::decode(std::string& string)
{
    uint32_t strlen;
    decode(strlen);
    if (strlen > nbytes)
        throw std::runtime_error("Buffer-read overflow");
    string.assign(next, strlen);
    const size_t len = roundup(strlen);
    nbytes -= len;
    next += len;
}

size_t Decoder::getDmaSize()
{
    return getSize() - nbytes;
}

void Encoder::encode(
        const void* const bytes,
        const size_t      len)
{
    if (dma.iov_len)
        throw std::runtime_error("I/O vector overflow");
    dma.iov_base = const_cast<void*>(bytes);
    dma.iov_len = len;
}

size_t Decoder::decode(
        void* const  bytes,
        const size_t len)
{
    struct iovec iov[2];
    iov[0].iov_base = buf;
    iov[0].iov_len = next - buf;
    iov[1].iov_base = bytes;
    iov[1].iov_len = len;
    const size_t nbytes = read(iov, 2);
    clear();
    return nbytes;
}

void Encoder::write()
{
    struct iovec iov[2];
    iov[0].iov_base = buf;
    iov[0].iov_len = nbytes;
    iov[1] = dma;
    clear();
    write(iov, 2);
}

size_t Decoder::read(size_t nbytes)
{
    if (nbytes == 0) {
        nbytes = maxSize;
    }
    else if (nbytes > maxSize) {
        throw std::invalid_argument("Read-length too large: nbytes=" +
                std::to_string(nbytes) + ", max=" + std::to_string(maxSize));
    }
    clear();
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = nbytes;
    return this->nbytes = read(&iov, 1, true);
}

} // namespace
