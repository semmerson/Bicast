/**
 * This file implements a data segment
 *
 *  @file:  DataSeg.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include "error.h"
#include "HycastProto.h"
#include "Socket.h"

#include <memory>

namespace hycast {

class DataSeg::Impl
{
public:
    DataSegId   segId;    ///< Data-segment identifier
    /// Product size in bytes (for when product notice is missed)
    ProdSize    prodSize;
    SegSize     bufSize;  ///< Size of buffer in bytes
    char*       buf;      ///< buffer for data

    Impl()
        : segId()
        , prodSize(0)
        , bufSize(0)
        , buf(nullptr)
    {}

    Impl(const DataSegId& segId,
         const ProdSize   prodSize,
         const char*      data)
        : segId(segId)
        , prodSize(prodSize)
        , bufSize(DataSeg::size(prodSize, segId.offset))
        , buf(new char[bufSize])
    {
        (void)::memcpy(buf, data, bufSize);
    }

    virtual ~Impl() noexcept {
        delete[] buf;
    }

    const char* data() const noexcept {
        return buf;
    }

    String to_string(const bool withName) const {
        String string;
        if (withName)
            string += "DataSeg";
        return string + "{segId=" + segId.to_string() + ", prodSize=" +
                std::to_string(prodSize) + "}";
    }

    bool read(TcpSock& sock) {
        bool success = segId.read(sock) && sock.read(prodSize);

        if (success) {
            auto segSize = DataSeg::size(prodSize, segId.offset);
            if (bufSize < segSize) {
                delete[] buf;
                buf = new char[segSize];
                bufSize = segSize;
            }
            success = sock.read(buf, segSize);
        }

        return success;
    }

    bool write(TcpSock& sock) {
        return segId.write(sock) && sock.write(prodSize) &&
            sock.write(buf, DataSeg::size(prodSize, segId.offset));
    }

    bool read(UdpSock& sock) {
        /*
        bool success = segId.read(sock);

        if (success) {
            sock.addPeek(prodSize);
            success = sock.peek();

            if (success) {
                auto segSize = DataSeg::size(prodSize, segId.offset);
                if (bufSize < segSize) {
                    delete[] buf;
                    buf = new char[segSize];
                    bufSize = segSize;
                }
                sock.addPeek(buf, segSize);
                success = sock.peek();
            }
        }

        return success;
        */
        return true;
    }

    bool write(UdpSock& sock) {
        return true;
        /*
        return segId.write(sock) && sock.write(prodSize) &&
            sock.write(buf, DataSeg::size(prodSize, segId.offset));
        */
    }
};

class SockSeg final : public DataSeg::Impl
{
    char* buf;

public:
    SockSeg(TcpSock& sock)
        : Impl()
        , buf(nullptr)
    {
        bool success = false;
        if (segId.read(sock) && sock.read(prodSize)) {
            auto nbytes = DataSeg::size(prodSize, segId.offset);
            buf = new char[nbytes];
            Impl::buf = buf;
            success = sock.read(buf, nbytes);
        }
        if (!success)
            throw EOF_ERROR("EOF encountered reading data-segment " +
                    segId.to_string());
    }

    ~SockSeg() noexcept {
        delete[] buf;
    }
};

/******************************************************************************/

DataSeg::DataSeg()
    : pImpl{}
{}

DataSeg::DataSeg(const DataSegId& segId,
                 const ProdSize   prodSize,
                 const char*      data)
    : pImpl(std::make_shared<Impl>(segId, prodSize, data))
{}

DataSeg::DataSeg(TcpSock& sock)
    : pImpl(std::make_shared<SockSeg>(sock))
{}

DataSeg::operator bool() const {
    return static_cast<bool>(pImpl);
}

const DataSegId& DataSeg::getId() const noexcept {
    return pImpl->segId;
}

ProdSize DataSeg::getProdSize() const noexcept {
    return pImpl->prodSize;
}

const char* DataSeg::getData() const noexcept {
    return pImpl->buf;
}

String DataSeg::to_string(const bool withName) const {
    return pImpl->to_string(withName);
}

bool DataSeg::read(TcpSock& sock) const {
    return pImpl->read(sock);
}

bool DataSeg::write(TcpSock& sock) const {
    return pImpl->write(sock);
}

bool DataSeg::read(UdpSock& sock) const {
    return pImpl->read(sock);
}

bool DataSeg::write(UdpSock& sock) const {
    return pImpl->write(sock);
}

} // namespace
