/**
 * This file  
 *
 *  @file:  
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

#include <cstring>
#include <ctime>

namespace hycast {

std::string DataSegId::to_string(const bool withName) const
{
    String string;
    if (withName)
        string += "DataSegId";
    return string + "{prodIndex=" + prodIndex.to_string() +
            ", offset=" + std::to_string(offset) + "}";
}

String NoteReq::to_string() const {
    return (id == Id::PROD_INDEX)
            ? prodIndex.to_string()
            : (id == Id::DATA_SEG_ID)
                ? dataSegId.to_string()
                : "<unset>";
}

size_t NoteReq::hash() const noexcept {
    return (id == Id::PROD_INDEX)
            ? prodIndex.hash()
            : (id == Id::DATA_SEG_ID)
                ? dataSegId.hash()
                : 0;
}

bool NoteReq::operator==(const NoteReq& rhs) const noexcept {
    if (id != rhs.id)    return false;
    if (id == Id::UNSET) return true;
    return (id == Id::PROD_INDEX)
            ? prodIndex == rhs.prodIndex
            : dataSegId == rhs.dataSegId;
}

std::string Timestamp::to_string(const bool withName) const
{
    const time_t time = sec;
    const auto   timeStruct = *::gmtime(&time);
    char         buf[40];
    auto         nbytes = ::strftime(buf, sizeof(buf), "%FT%T.", &timeStruct);
    ::snprintf(buf+nbytes, sizeof(buf)-nbytes, "%06luZ}",
            static_cast<unsigned long>(nsec/1000));

    String string;
    if (withName)
        string += "TimeStamp{";
    string += buf;
    if (withName)
        string += "}";

    return string;
}

/******************************************************************************/

/// Product information
class ProdInfo::Impl
{
    friend class ProdInfo;

    ProdIndex index;   ///< Product index
    String    name;    ///< Name of product
    ProdSize  size;    ///< Size of product in bytes

public:
    Impl() =default;

    Impl(    const ProdIndex   index,
             const std::string name,
             const ProdSize    size)
        : index{index}
        , name(name)
        , size{size}
    {}

    bool operator==(const Impl& rhs) const {
        return index == rhs.index &&
               name == rhs.name &&
               size == rhs.size;
    }

    String to_string(const bool withName) const
    {
        String string;
        if (withName)
            string += "ProdInfo";
        return string + "{index=" + index.to_string() + ", name=\"" + name +
                "\", size=" + std::to_string(size) + "}";
    }

    bool write(Xprt& xprt) const {
        LOG_NOTE("Writing product information to %s", xprt.to_string().data());
        auto success = index.write(xprt);
        if (success) {
            LOG_NOTE("Writing product name to %s", xprt.to_string().data());
            success = xprt.write(name);
            if (success) {
                LOG_NOTE("Writing product size to %s", xprt.to_string().data());
                success = xprt.write(size);
            }
        }
        return success;
    }

    bool read(Xprt& xprt) {
        LOG_NOTE("Reading product information from %s", xprt.to_string().data());
        auto success = index.read(xprt);
        if (success) {
            LOG_NOTE("Reading product name from %s", xprt.to_string().data());
            success = xprt.read(name);
            if (success) {
                LOG_NOTE("Reading product size from %s", xprt.to_string().data());
                success = xprt.read(size);
            }
        }
        return success;
    }
};

ProdInfo::ProdInfo() =default;

ProdInfo::ProdInfo(const ProdIndex   index,
                   const std::string name,
                   const ProdSize    size)
    : pImpl{std::make_shared<Impl>(index, name, size)}
{}

ProdInfo::operator bool() const {
    return static_cast<bool>(pImpl);
}

const ProdIndex& ProdInfo::getIndex() const {
    return pImpl->index;
}
const String&    ProdInfo::getName() const {
    return pImpl->name;
}
const ProdSize&  ProdInfo::getSize() const {
    return pImpl->size;
}

bool ProdInfo::operator==(const ProdInfo& rhs) const {
    return pImpl->operator==(*rhs.pImpl);
}

String ProdInfo::to_string(const bool withName) const {
    return pImpl->to_string(withName);
}

bool ProdInfo::write(Xprt& xprt) const {
    return pImpl->write(xprt);
}

bool ProdInfo::read(Xprt& xprt) {
    if (!pImpl)
        pImpl = std::make_shared<Impl>();
    return pImpl->read(xprt);
}

/******************************************************************************
 * Transport module
 ******************************************************************************/

class Xprt::Impl
{
protected:
    Socket      sock;

    inline bool write(PduId pduId) {
        return sock.write(static_cast<PduType>(pduId));
    }

    inline bool read(PduId& pduId) {
        PduType id;
        if (!sock.read(id))
            return false;
        pduId = static_cast<PduId>(id);
        return true;
    }

public:
    Impl(Socket& sock)
        : sock(sock)
    {}

    virtual ~Impl() {};

    SockAddr getRmtAddr() const {
        return sock.getRmtAddr();
    }

    String to_string() const {
        return sock.to_string();
    }

    /**
     * Sends a boolean as a PDU to the remote counterpart.
     *
     * @param[in] pduId    PDU ID
     * @param[in] value    Boolean to be sent
     * @param[in] xprt     Transport
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    virtual bool send(PduId      pduId,
                      const bool value) =0;

    /**
     * Sends an object as a PDU to the remote counterpart.
     *
     * @param[in] pduId    PDU ID
     * @param[in] obj      Object to be sent
     * @param[in] xprt     Transport
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    virtual bool send(PduId           pduId,
                      const XprtAble& obj,
                      Xprt&           xprt) =0;

    /**
     * Receives the next PDU.
     *
     * @param[out] pduId    Identifier of the next PDU
     * @retval     `true`   Success
     * @retval     `false`  Connection lost
     */
    virtual bool recv(PduId& pduId) =0;

    bool write(const void* value,
               size_t      nbytes) {
        return sock.write(value, nbytes);
    }
    bool write(const bool value) {
        return sock.write(value);
    }
    bool write(const uint8_t  value) {
        return sock.write(value);
    }
    bool write(const uint16_t value) {
        return sock.write(value);
    }
    bool write(const uint32_t value) {
        LOG_NOTE("Writing uint32_t");
        return sock.write(value);
    }
    bool write(const uint64_t value) {
        return sock.write(value);
    }
    bool write(const String&  value) {
        return sock.write(value);
    }

    bool read(void*  value,
              size_t nbytes) {
        return sock.read(value, nbytes);
    }
    bool read(bool&     value) {
        return sock.read(value);
    }
    bool read(uint8_t&  value) {
        return sock.read(value);
    }
    bool read(uint16_t& value) {
        return sock.read(value);
    }
    bool read(uint32_t& value) {
        LOG_NOTE("Reading uint32_t");
        return sock.read(value);
    }
    bool read(uint64_t& value) {
        return sock.read(value);
    }
    bool read(String&   value) {
        return sock.read(value);
    }

    void shutdown() {
        sock.shutdown(SHUT_RD);
    }
};

/******************************************************************************/

class TcpXprt final : public Xprt::Impl
{
public:
    TcpXprt(TcpSock& sock)
        : Xprt::Impl(sock)
    {}

    bool send(PduId      pduId,
              const bool value) override {
        return write(pduId) && sock.write(value);
    }

    bool send(PduId pduId, const XprtAble& obj, Xprt& xprt) override {
        return write(pduId) && obj.write(xprt);
    }

    bool recv(PduId& pduId) override {
        return read(pduId);
    }
};

/******************************************************************************/

class UdpXprt final : public Xprt::Impl
{
    bool flush() {
        return static_cast<UdpSock*>(&sock)->flush();
    }

    void clear() {
        return static_cast<UdpSock*>(&sock)->clear();
    }

public:
    UdpXprt(UdpSock& sock)
        : Xprt::Impl(sock)
    {}

    bool send(PduId      pduId,
              const bool value) override {
        return write(pduId) && sock.write(value) && flush();
    }

    bool send(PduId pduId, const XprtAble& obj, Xprt& xprt) override {
        return write(pduId) && obj.write(xprt) && flush();
    }

    bool recv(PduId& pduId) override {
        clear();
        return read(pduId);
    }
};

/******************************************************************************/

Xprt::Xprt(TcpSock& sock)
    : pImpl(new TcpXprt(sock))
{}

Xprt::Xprt(UdpSock& sock)
    : pImpl(new UdpXprt(sock))
{}

SockAddr Xprt::getRmtAddr() const {
    return pImpl->getRmtAddr();
}

String Xprt::to_string() const {
    return pImpl ? pImpl->to_string() : "<unset>";
}

bool Xprt::send(PduId pduId, const bool value) const {
    return pImpl->send(pduId, value);
}

bool Xprt::send(PduId pduId, const XprtAble& obj) {
    return pImpl->send(pduId, obj, *this);
}

bool Xprt::recv(PduId& pduId) {
    return pImpl->recv(pduId);
}

bool Xprt::write(const void* value,
           size_t      nbytes) {
    return pImpl->write(value, nbytes);
}
bool Xprt::write(const bool value) {
    return pImpl->write(value);
}
bool Xprt::write(const uint8_t  value) {
    return pImpl->write(value);
}
bool Xprt::write(const uint16_t value) {
    return pImpl->write(value);
}
bool Xprt::write(const uint32_t value) {
    return pImpl->write(value);
}
bool Xprt::write(const uint64_t value) {
    return pImpl->write(value);
}
bool Xprt::write(const String&  value) {
    return pImpl->write(value);
}

bool Xprt::read(void*  value,
          size_t nbytes) {
    return pImpl->read(value, nbytes);
}
bool Xprt::read(bool&     value) {
    return pImpl->read(value);
}
bool Xprt::read(uint8_t&  value) {
    return pImpl->read(value);
}
bool Xprt::read(uint16_t& value) {
    return pImpl->read(value);
}
bool Xprt::read(uint32_t& value) {
    return pImpl->read(value);
}
bool Xprt::read(uint64_t& value) {
    return pImpl->read(value);
}
bool Xprt::read(String&   value) {
    return pImpl->read(value);
}

void Xprt::shutdown() {
    return pImpl->shutdown();
}

} // namespace

namespace std {
    string to_string(const hycast::ProdInfo& prodInfo) {
        return prodInfo.to_string();
    }
}
