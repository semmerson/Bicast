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
#include "config.h"

#include "CommonTypes.h"

#include <error.h>
#include <cstddef>
#include <cstdint>
#include <Socket.h>
#include <string>
#include <Xprt.h>

namespace hycast {

/// An implementation of a transport
class Xprt::Impl
{
    Socket sock;

    inline bool hton(const bool value) {
        return value;
    }

    inline uint8_t hton(const uint8_t value) {
        return value;
    }

    inline uint16_t hton(const uint16_t value) {
        return htons(value);
    }

    inline uint32_t hton(const uint32_t value) {
        return htonl(value);
    }

    inline uint64_t hton(uint64_t value) {
        uint64_t  v64;
        uint32_t* v32 = reinterpret_cast<uint32_t*>(&v64);

        v32[0] = htonl(static_cast<uint32_t>(value >> 32));
        v32[1] = htonl(static_cast<uint32_t>(value));

        return v64;
    }

    inline bool ntoh(const bool value)
    {
        return value;
    }

    inline uint8_t ntoh(const uint8_t value)
    {
        return value;
    }

    inline uint16_t ntoh(const uint16_t value)
    {
        return ntohs(value);
    }

    inline uint32_t ntoh(const uint32_t value)
    {
        return ntohl(value);
    }

    inline uint64_t ntoh(uint64_t value)
    {
        uint32_t* v32 = reinterpret_cast<uint32_t*>(&value);
        return (static_cast<uint64_t>(ntohl(v32[0])) << 32) | ntohl(v32[1]);
    }

public:
    /**
     * Constructs from a socket.
     * @param[in] sock  The socket
     */
    Impl(Socket sock)
        : sock(sock)
    {}

    virtual ~Impl() {};

    /**
     * Returns the underlying socket.
     * @return The underlying socket
     */
    Socket getSocket() const noexcept {
        return sock;
    }

    /**
     * Returns the socket address of the remote endpoint.
     * @return The socket address of the remote endpoint
     */
    SockAddr getRmtAddr() const noexcept {
        return sock.getRmtAddr();
    }

    /**
     * Returns the socket address of the local endpoint.
     * @return The socket address of the local endpoint
     */
    SockAddr getLclAddr() const {
        return sock.getLclAddr();
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    std::string to_string() const {
        return sock.to_string();
    }

    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
    size_t hash() const {
        return sock.hash();
    }

    /**
     * Write functions:
     */

    /**
     * Writes a boolean value.
     * @param[in] value    Value to be written
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(const bool         value) {
        //LOG_DEBUG("Writing boolean value to %s", to_string().data());
        return sock.write(value);
    }
    /**
     * Writes an unsigned, 8-bit value.
     * @param[in] value    Value to be written
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(const uint8_t      value) {
        //LOG_DEBUG("Writing 1-byte value to %s", to_string().data());
        return sock.write(value);
    }
    /**
     * Writes bytes.
     * @param[in] bytes    Bytes to be written
     * @param[in] nbytes   Number of bytes to write
     * @retval    true     Success
     * @retval    false    Connection lost
     */
    bool write(
            const void*  bytes,
            const size_t nbytes) {
        return sock.write(bytes, nbytes);
    }
    /**
     * Performs byte-order translation.
     *
     * @tparam  Type of unsigned integer to be written
     */
    template<class UINT>
    bool write(UINT value) {
        //LOG_DEBUG("Writing %zu-byte value to %s", sizeof(value), to_string().data());
        value = hton(value);
        return sock.write(value);
    }
    /**
     * @tparam    UINT     Type of serialized, unsigned integer to hold string
     *                     length
     * @param[in] string   String to be written
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    template<typename UINT>
    bool write(const std::string& string) {
        static const UINT max = ~static_cast<UINT>(0);
        //LOG_DEBUG("Writing \"%s\" to %s", string.data(), to_string().data());
        if (string.size() > max)
            throw INVALID_ARGUMENT("String is longer than " + std::to_string(max) + " bytes");
        const UINT size = static_cast<UINT>(string.size());
        return sock.write(hton(size)) && sock.write(string.data(), size);
    }

    /**
     * Writes a system time-point.
     * @param[in] time     The system time-point to be written
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const SysTimePoint& time) {
        uint64_t secs = SysTimePoint::clock::to_time_t(time);
        int32_t  usecs = std::chrono::duration_cast<std::chrono::microseconds>(
            time - SysClock::from_time_t(secs)).count();
        if (usecs < 0) {
            --secs;
            usecs += 1000000;
        }
        return write(secs) && write<uint32_t>(*reinterpret_cast<uint32_t*>(&usecs));
    }

#if 0
    inline bool operator<<(const uint32_t value) {
        return sock.write(hton(value));
    }

    bool operator<<(const int32_t value) {
        // Using a pointer is necessary to ensure the bit-pattern is unchanged
        return  *this << *reinterpret_cast<uint32_t*>(&value);
    }

    inline bool operator>>(uint32_t& value) {
        if (!sock.read(value))
            return false;
        value = ntoh(value);
        return true;
    }

    bool operator>>(int32_t& value) {
        // Using a pointer is necessary to ensure the bit-pattern is unchanged
        return *this >> *reinterpret_cast<uint32_t*>(&value);
    }

    inline bool operator<<(const uint64_t value) {
        return sock.write(hton(value));
    }

    bool operator<<(const int64_t value) {
        // Using a pointer is necessary to ensure the bit-pattern is unchanged
        return  *this << *reinterpret_cast<uint64_t*>(&value);
    }

    inline bool operator>>(uint64_t& value) {
        if (!sock.read(value))
            return false;
        value = ntoh(value);
        return true;
    }

    bool operator>>(int64_t& value) {
        // Using a pointer is necessary to ensure the bit-pattern is unchanged
        return *this >> *reinterpret_cast<uint64_t*>(&value);
    }
#endif

    /**
     * Flushes the underlying socket.
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool flush() {
        return sock.flush();
    }

    /**
     * Read functions:
     */

    /**
     * Reads a boolean value.
     * @param[out] value    Value to be read
     * @retval     true     Success
     * @retval     false    Lost connection
     */
    bool read(bool&        value) {
        if (sock.read(value)) {
            //LOG_DEBUG("Read boolean value from %s", to_string().data());
            return true;
        }
        return false;
    }
    /**
     * Reads an unsigned, 8-bit value.
     * @param[out] value    Value to be read
     * @retval     true     Success
     * @retval     false    Lost connection
     */
    bool read(uint8_t&     value) {
        if (sock.read(value)) {
            //LOG_DEBUG("Read 1-byte value from %s", to_string().data());
            return true;
        }
        return false;
    }
    /**
     * Reads bytes.
     * @param[out] value    Bytes to be set
     * @param[in]  nbytes   Number of bytes to read
     * @retval     true     Success
     * @retval     false    Lost connection
     */
    bool read(void*        value,
              size_t       nbytes) {
        return sock.read(value, nbytes);
    }
    /**
     * Performs byte-order translation
     *
     * @tparam  Type of primitive value to be read
     */
    template<class TYPE>
    bool read(TYPE& value) {
        if (sock.read(value)) {
            value = ntoh(value);
            //LOG_DEBUG("Read %zu-byte value from %s", sizeof(value),
                    //to_string().data());
            return true;
        }
        return false;
    }
    /**
     * @tparam    UINT     Type of serialized, unsigned integer that holds
     *                     string length
     * @param[in] string   String to be read
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    template<typename UINT>
    bool read(std::string& string) {
        UINT size;
        if (sock.read(size)) {
            size = ntoh(size);
            char bytes[size];
            if (sock.read(bytes, sizeof(bytes))) {
                string.assign(bytes, size);
                //LOG_DEBUG("Read \"%s\" from %s", string.data(),
                        //to_string().data());
                return true;
            }
        }
        return false;
    }

    /**
     * Reads a system time-point.
     * @param[in] time     The system time-point to be set
     * @retval    true     Success. `time` is set.
     * @retval    false    Lost connection
     */
    bool read(SysTimePoint& time) {
        uint64_t secs;
        uint32_t usecs;

        const auto success = read(secs) && read(usecs);

        if (success)
            time = SysTimePoint::clock::from_time_t(secs) + std::chrono::microseconds(usecs);

        return success;
    }

    /**
     * Clears the underlying socket for the next read. Does nothing for TCP. Deletes the datagram for UDP.
     */
    void clear() {
        sock.clear();
    }

    /**
     * Shuts down reading from the underlying socket.
     */
    void shutdown() {
        sock.shutdown(SHUT_RD);
    }
};

/******************************************************************************/

Xprt::Xprt(Socket sock)
    : pImpl(new Impl(sock))
{}

/*
Xprt::Xprt(const Xprt& xprt);

Xprt::Xprt& operator=(const Xprt& rhs);

Xprt::~Xprt() noexcept;
*/

Socket Xprt::getSocket() const {
    return pImpl->getSocket();
}

SockAddr Xprt::getRmtAddr() const noexcept {
    return pImpl->getRmtAddr();
}

SockAddr Xprt::getLclAddr() const {
    return pImpl->getLclAddr();
}

std::string Xprt::to_string() const {
    return pImpl ? pImpl->to_string() : "<unset>";
}

size_t Xprt::hash() const {
    return pImpl ? pImpl->hash() : 0;
}

void Xprt::swap(Xprt& xprt) noexcept {
    /*
    auto tmp = xprt.pImpl;
    xprt.pImpl = pImpl;
    pImpl = tmp;
    */
    pImpl.swap(xprt.pImpl);
}

bool Xprt::write(const void*        value,
           size_t                   nbytes) const {
    return pImpl->write(value, nbytes);
}
bool Xprt::write(const bool         value) const {
    return pImpl->write(value);
}
bool Xprt::write(const uint8_t      value) const {
    return pImpl->write(value);
}
bool Xprt::write(const int8_t       value) const {
    return pImpl->write<uint8_t>(*reinterpret_cast<const uint8_t*>(&value));
}
bool Xprt::write(const uint16_t     value) const {
    return pImpl->write<uint16_t>(value);
}
bool Xprt::write(const int16_t      value) const {
    return pImpl->write<uint16_t>(*reinterpret_cast<const uint16_t*>(&value));
}
bool Xprt::write(const uint32_t     value) const {
    return pImpl->write<uint32_t>(value);
}
bool Xprt::write(const int32_t      value) const {
    return pImpl->write<uint32_t>(*reinterpret_cast<const uint32_t*>(&value));
}
bool Xprt::write(const uint64_t     value) const {
    return pImpl->write<uint64_t>(value);
}
bool Xprt::write(const int64_t      value) const {
    return pImpl->write<uint64_t>(*reinterpret_cast<const uint64_t*>(&value));
}
template<typename UINT>
bool Xprt::write(const std::string& string) const {
    return pImpl->write<UINT>(string);
}
template bool Xprt::write<uint8_t >(const std::string& string) const;
template bool Xprt::write<uint16_t>(const std::string& string) const;
template bool Xprt::write<uint32_t>(const std::string& string) const;
template bool Xprt::write<uint64_t>(const std::string& string) const;
bool Xprt::write(const SysTimePoint& time) const {
    return pImpl->write(time);
}

bool Xprt::flush() const {
    return pImpl->flush();
}

bool Xprt::read(void*        value,
                size_t       nbytes) const {
    return pImpl->read(value, nbytes);
}
bool Xprt::read(bool&        value) const {
    return pImpl->read(value);
}
bool Xprt::read(uint8_t&     value) const {
    return pImpl->read(value);
}
bool Xprt::read(uint16_t&    value) const {
    return pImpl->read<uint16_t>(value);
}
bool Xprt::read(int16_t&     value) const {
    return pImpl->read<uint16_t>(*reinterpret_cast<uint16_t*>(&value));
}
bool Xprt::read(uint32_t&    value) const {
    return pImpl->read<uint32_t>(value);
}
bool Xprt::read(uint64_t&    value) const {
    return pImpl->read<uint64_t>(value);
}
template<typename UINT>
bool Xprt::read(std::string& string) const {
    return pImpl->read<UINT>(string);
}
template bool Xprt::read<uint8_t>(std::string& string) const;
template bool Xprt::read<uint16_t>(std::string& string) const;
template bool Xprt::read<uint32_t>(std::string& string) const;
template bool Xprt::read<uint64_t>(std::string& string) const;
bool Xprt::read(SysTimePoint& time) const {
    return pImpl->read(time);
}

void Xprt::clear() const {
    return pImpl->clear();
}

void Xprt::shutdown() {
    return pImpl->shutdown();
}

} // namespace
