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
    : serialBufSize{maxSize}
    , serialBuf{new char[maxSize]}
    , nextSerial{serialBuf}
{
    dma.iov_len = 0;
}

Codec::~Codec()
{
    delete[] serialBuf;
}

inline size_t Codec::roundup(const size_t len)
{
    return ((len + alignment - 1)/alignment) * alignment;
}

void Codec::clear() noexcept
{
    serialBufBytes = 0;
    nextSerial = serialBuf;
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
{
    serialBufBytes = 0;
}

Encoder::~Encoder()
{}

Decoder::Decoder(const size_t maxSize)
    : Codec{maxSize}
{
    serialBufBytes = maxSize;
}

Decoder::~Decoder()
{}

void Encoder::encode(uint16_t value)
{
    const size_t len = roundup(sizeof(uint16_t));
    if (serialBufBytes + len > serialBufSize)
        throw std::runtime_error("Buffer-write overflow");
    *reinterpret_cast<uint16_t*>(nextSerial) = htons(value);
    serialBufBytes += len;
    nextSerial += len;
}

void Encoder::encode(uint32_t value)
{
    const size_t len = roundup(sizeof(uint32_t));
    if (serialBufBytes + len > serialBufSize)
        throw std::runtime_error("Buffer-write overflow");
    *reinterpret_cast<uint32_t*>(nextSerial) = htonl(value);
    serialBufBytes += len;
    nextSerial += len;
}

void Decoder::decode(uint16_t& value)
{
    const size_t len = roundup(sizeof(uint16_t));
    if (serialBufBytes < len)
        throw std::runtime_error("Buffer-read overflow");
    value = ntohs(*reinterpret_cast<uint16_t*>(nextSerial));
    serialBufBytes -= len;
    nextSerial += len;
}

void Decoder::decode(uint32_t& value)
{
    const size_t len = roundup(sizeof(uint32_t));
    if (serialBufBytes < len)
        throw std::runtime_error("Buffer-read overflow");
    value = ntohl(*reinterpret_cast<uint32_t*>(nextSerial));
    serialBufBytes -= len;
    nextSerial += len;
}

void Encoder::encode(const std::string& string)
{
    const size_t strlen = string.size();
    if (strlen > UINT32_MAX)
        throw std::invalid_argument("String too long: len=" +
                std::to_string(strlen));
    encode(static_cast<uint32_t>(strlen));
    const size_t len = roundup(strlen);
    if (serialBufBytes + len > serialBufSize)
        throw std::runtime_error("Buffer-write overflow");
    memcpy(nextSerial, string.data(), strlen);
    serialBufBytes += len;
    nextSerial += len;
}

void Decoder::decode(std::string& string)
{
    uint32_t strlen;
    decode(strlen);
    if (strlen > serialBufBytes)
        throw std::runtime_error("Buffer-read overflow");
    string.assign(nextSerial, strlen);
    const size_t len = roundup(strlen);
    serialBufBytes -= len;
    nextSerial += len;
}

size_t Decoder::getDmaSize()
{
    return getSize() - (nextSerial - serialBuf);
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
    iov[0].iov_base = serialBuf;
    iov[0].iov_len = nextSerial - serialBuf;
    iov[1].iov_base = bytes;
    iov[1].iov_len = len;
    const size_t nbytes = read(iov, 2);
    clear();
    return nbytes;
}

void Encoder::flush()
{
    struct iovec iov[2];
    iov[0].iov_base = serialBuf;
    iov[0].iov_len = serialBufBytes;
    iov[1] = dma;
    clear();
    write(iov, 2);
}

size_t Decoder::fill(size_t nbytes)
{
    if (nbytes == 0) {
        nbytes = serialBufSize;
    }
    else if (nbytes > serialBufSize) {
        throw std::invalid_argument("Read-length too large: nbytes=" +
                std::to_string(nbytes) + ", max=" + std::to_string(serialBufSize));
    }
    clear();
    struct iovec iov;
    iov.iov_base = serialBuf;
    iov.iov_len = nbytes;
    return this->serialBufBytes = read(&iov, 1, true);
}

MemEncoder::MemEncoder(
            char* const  buf,
            const size_t maxSize)
    : Encoder(maxSize)
    , buf{buf}
    , size{0}
{}

void MemEncoder::write(
        const struct iovec* iov,
        const int           iovcnt)
{
    char* next = buf + size;
    size_t left = serialBufSize - size;
    for (int i = 0; left > 0 && i < iovcnt; ++i, ++iov) {
        const size_t len = iov->iov_len > left ? left : iov->iov_len;
        ::memcpy(next, iov->iov_base, len);
        next += len;
        left -= len;
    }
    size = next - buf;
}

MemDecoder::MemDecoder(
        const char* const  memBuf,
        const size_t       bufLen)
    : Decoder(bufLen)
    , memBuf{memBuf}
    , memRead{0}
{}

size_t MemDecoder::getSize()
{
    return serialBufSize;
}

size_t MemDecoder::read(
        const struct iovec* iov,
        const int           iovcnt,
        const bool          peek)
{
    const char* const start = memBuf + memRead;
    const char*       next = start;
    size_t            left = serialBufSize - memRead;
    for (int i = 0; left > 0 && i < iovcnt; ++i, ++iov) {
        const size_t len = iov->iov_len > left ? left : iov->iov_len;
        ::memcpy(iov->iov_base, next, len);
        next += len;
        left -= len;
    }
    if (!peek)
        memRead = next - memBuf;
    return next - start;
}

bool MemDecoder::hasRecord()
{
    return memRead < serialBufSize;
}

void MemDecoder::discard()
{
    clear();
}

} // namespace
