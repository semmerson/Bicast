/**
 * This file defines a socket-based transport mechanism that is independent of
 * the socket's underlying protocol (TCP, UDP, etc.).
 *
 *  @file:  Xprt.cpp
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

#include <error.h>
#include <Socket.h>
#include <Xprt.h>

namespace hycast {

class Xprt::Impl
{
    using PduId = Xprt::PduId;

    Socket   sock;
    Dispatch dispatch;

public:
    Impl(Socket& sock)
        : sock(sock)
        , dispatch()
    {}

    Impl(Socket& sock, Dispatch& dispatch)
        : sock(sock)
        , dispatch(dispatch)
    {}

    virtual ~Impl() {};

    SockAddr getRmtAddr() const {
        return sock.getRmtAddr();
    }

    SockAddr getLclAddr() const {
        return sock.getLclAddr();
    }

    std::string to_string() const {
        return sock.to_string();
    }

    /**
     * Sends a PDU ID as a PDU to the remote counterpart.
     *
     * @param[in] pduId    PDU ID
     * @param[in] xprt     Transport
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool send(const PduId pduId,
              Xprt&       xprt) {
        return xprt.write(pduId) && sock.flush();
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
    bool send(const PduId pduId,
              const bool  value,
              Xprt&       xprt) {
        return xprt.write(pduId) && sock.write(value) && sock.flush();
    }

    /**
     * Sends an object as a PDU to the remote counterpart.
     *
     * @param[in] pduId    PDU ID
     * @param[in] obj      Object to be sent
     * @param[in] xprt     Transport
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool send(PduId            pduId,
              const WriteAble& obj,
              Xprt&            xprt) {
        return xprt.write(pduId) && obj.write(xprt) && sock.flush();
    }

    bool recv(Xprt xprt, Dispatch& dispatch) {
        PduId id;
        sock.clear();
        return read(id) && dispatch(id, xprt);
    }

    bool write(const void*        value,
               size_t             nbytes) {
        return sock.write(value, nbytes);
    }
    bool write(const bool         value) {
        return sock.write(value);
    }
    bool write(const uint8_t      value) {
        return sock.write(value);
    }
    bool write(const int16_t      value) {
        return sock.write(value);
    }
    bool write(const uint16_t     value) {
        return sock.write(value);
    }
    bool write(const int32_t      value) {
        return sock.write(value);
    }
    bool write(const uint32_t     value) {
        return sock.write(value);
    }
    bool write(const uint64_t     value) {
        return sock.write(value);
    }
    bool write(const std::string& value) {
        return sock.write(value);
    }

    bool read(void*        value,
              size_t       nbytes) {
        return sock.read(value, nbytes);
    }
    bool read(bool&        value) {
        return sock.read(value);
    }
    bool read(uint8_t&     value) {
        return sock.read(value);
    }
    bool read(uint16_t&    value) {
        return sock.read(value);
    }
    bool read(int32_t&     value) {
        return sock.read(value);
    }
    bool read(uint32_t&    value) {
        return sock.read(value);
    }
    bool read(uint64_t&    value) {
        return sock.read(value);
    }
    bool read(std::string& value) {
        return sock.read(value);
    }

    void shutdown() {
        sock.shutdown(SHUT_RD);
    }
};

/******************************************************************************/

Xprt::Xprt(Socket& sock)
    : pImpl(new Impl(sock))
{}

Xprt::Xprt(Socket&& sock)
    : pImpl(new Impl(sock))
{}

SockAddr Xprt::getRmtAddr() const {
    return pImpl->getRmtAddr();
}

SockAddr Xprt::getLclAddr() const {
    return pImpl->getLclAddr();
}

std::string Xprt::to_string() const {
    return pImpl ? pImpl->to_string() : "<unset>";
}

bool Xprt::send(const PduId pduId) {
    return pImpl->send(pduId, *this);
}

bool Xprt::send(const PduId pduId, const bool value) {
    return pImpl->send(pduId, value, *this);
}

bool Xprt::send(const PduId pduId, const WriteAble& obj) {
    return pImpl->send(pduId, obj, *this);
}

bool Xprt::recv(Dispatch& dispatch) {
    return pImpl->recv(*this, dispatch);
}

bool Xprt::write(const void*        value,
           size_t                   nbytes) {
    return pImpl->write(value, nbytes);
}
bool Xprt::write(const bool         value) {
    return pImpl->write(value);
}
bool Xprt::write(const uint8_t      value) {
    return pImpl->write(value);
}
bool Xprt::write(const uint16_t     value) {
    return pImpl->write(value);
}
bool Xprt::write(const int32_t      value) {
    return pImpl->write(value);
}
bool Xprt::write(const uint32_t     value) {
    return pImpl->write(value);
}
bool Xprt::write(const uint64_t     value) {
    return pImpl->write(value);
}
bool Xprt::write(const std::string& value) {
    return pImpl->write(value);
}

bool Xprt::read(void*        value,
                size_t       nbytes) {
    return pImpl->read(value, nbytes);
}
bool Xprt::read(bool&        value) {
    return pImpl->read(value);
}
bool Xprt::read(uint8_t&     value) {
    return pImpl->read(value);
}
bool Xprt::read(uint16_t&    value) {
    return pImpl->read(value);
}
bool Xprt::read(int32_t&     value) {
    return pImpl->read(value);
}
bool Xprt::read(uint32_t&    value) {
    return pImpl->read(value);
}
bool Xprt::read(uint64_t&    value) {
    return pImpl->read(value);
}
bool Xprt::read(std::string& value) {
    return pImpl->read(value);
}

void Xprt::shutdown() {
    return pImpl->shutdown();
}

} // namespace
