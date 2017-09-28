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

#include "config.h"

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
    , serialBufBytes{0}
{
    dma.iov_len = 0;
    dma.iov_base = nullptr;
}

Codec::~Codec()
{
    delete[] serialBuf;
}

void Codec::reset() noexcept
{
    serialBufBytes = 0;
    nextSerial = serialBuf;
    dma.iov_len = 0;
}

size_t Codec::getSerialSize(const size_t size) noexcept
{
    return size;
}

size_t Codec::getSerialSize(const uint16_t* value)
{
    return sizeof(uint16_t);
}

size_t Codec::getSerialSize(const uint32_t* value)
{
    return sizeof(uint32_t);
}

size_t Codec::getSerialSize(const std::string& string)
{
    return sizeof(StrLen) + string.size();
}

Encoder::Encoder(
        const size_t maxSize)
    : Codec{maxSize}
{}

Encoder::~Encoder()
{}

Decoder::Decoder(
        const size_t maxSize)
    : Codec{maxSize}
{}

Decoder::~Decoder()
{}

size_t Encoder::encode(const uint16_t value)
{
    const size_t len = sizeof(uint16_t);
    if (serialBufBytes + len > serialBufSize)
        throw std::runtime_error("Buffer-write overflow");
    *reinterpret_cast<uint16_t*>(nextSerial) = htons(value);
    serialBufBytes += len;
    nextSerial += len;
    return len;
}

size_t Encoder::encode(const uint32_t value)
{
    const size_t len = sizeof(uint32_t);
    if (serialBufBytes + len > serialBufSize)
        throw std::runtime_error("Buffer-write overflow");
    *reinterpret_cast<uint32_t*>(nextSerial) = htonl(value);
    serialBufBytes += len;
    nextSerial += len;
    return len;
}

size_t Encoder::encode(const uint64_t value)
{
    const uint32_t* ptr = reinterpret_cast<const uint32_t*>(&value);
    return encode(ptr[0]) +  encode(ptr[1]);
}

void Decoder::decode(uint16_t& value)
{
    const size_t len = sizeof(uint16_t);
    if (serialBufBytes < len)
        throw std::runtime_error("Buffer-read overflow");
    value = ntohs(*reinterpret_cast<uint16_t*>(nextSerial));
    serialBufBytes -= len;
    nextSerial += len;
}

void Decoder::decode(uint32_t& value)
{
    const size_t len = sizeof(uint32_t);
    if (serialBufBytes < len)
        throw std::runtime_error("Buffer-read overflow");
    value = ntohl(*reinterpret_cast<uint32_t*>(nextSerial));
    serialBufBytes -= len;
    nextSerial += len;
}

void Decoder::decode(uint64_t& value)
{
    uint32_t* ptr = reinterpret_cast<uint32_t*>(&value);
    decode(ptr[0]);
    decode(ptr[1]);
}

size_t Encoder::encode(const std::string& string)
{
    const size_t strlen = string.size();
    if (strlen > maxStrLen)
        throw std::invalid_argument("String too long: len=" +
                std::to_string(strlen));
    size_t nbytes = encode(static_cast<StrLen>(strlen));
    if (serialBufBytes + strlen > serialBufSize)
        throw std::runtime_error("Buffer-write overflow");
    memcpy(nextSerial, string.data(), strlen);
    serialBufBytes += strlen;
    nextSerial += strlen;
    return nbytes + strlen;
}

void Decoder::decode(std::string& string)
{
    StrLen strlen;
    decode(strlen);
    if (strlen > serialBufBytes)
        throw std::runtime_error("Buffer-read overflow");
    string.assign(nextSerial, strlen);
    serialBufBytes -= strlen;
    nextSerial += strlen;
}

size_t Encoder::encode(
        const void* const bytes,
        const size_t      len)
{
    if (dma.iov_len)
        throw std::runtime_error("I/O vector overflow");
    dma.iov_base = const_cast<void*>(bytes);
    dma.iov_len = len;
    return len;
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
    reset();
    return nbytes - iov[0].iov_len; // Minus bytes in serial buffer
}

void Encoder::flush()
{
    struct iovec iov[2];
    iov[0].iov_base = serialBuf;
    iov[0].iov_len = nextSerial - serialBuf;
    iov[1] = dma;
    reset();
    write(iov, 2);
}

size_t Decoder::fill(size_t nbytes)
{
    ptrdiff_t have = nextSerial - serialBuf;
    if (nbytes == 0) {
        nbytes = serialBufSize - have;
    }
    else if (have + nbytes > serialBufSize) {
        throw std::invalid_argument("Read-length too large: nbytes=" +
                std::to_string(nbytes) + ", have=" +
                std::to_string(have) + ", serialBufSize=" +
                std::to_string(serialBufSize));
    }
    struct iovec iov;
    iov.iov_base = serialBuf;
    iov.iov_len = have + nbytes;
    nbytes = read(&iov, 1, true) - have;
    serialBufBytes += nbytes;
    return nbytes;
}

void Decoder::clear()
{
    discard();
    reset();
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

} // namespace
