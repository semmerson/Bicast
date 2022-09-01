/**
 * This file declares a socket-based transport mechanism that is independent of
 * the socket's underlying protocol (TCP, UCP, etc.).
 *
 *  @file:  Xprt.h
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

#ifndef MAIN_INET_XPRT_H_
#define MAIN_INET_XPRT_H_

#include "Socket.h"

#include <memory>

namespace hycast {

class Xprt
{
public:
    using PduId    = uint32_t;

    class Impl; // Implementation

private:
    std::shared_ptr<Impl> pImpl;

public:
    Xprt() =default;

    /**
     * Constructs.
     *
     * @param[in] sock      Socket
     */
    Xprt(Socket sock);

    /**
     * Copy constructs.
     *
     * @param[in] xprt  Other instance
    Xprt(const Xprt& xprt);
     */

    /**
     * Destroys.
    ~Xprt() noexcept;
     */

    /**
     * Copy Assigns.
     *
     * @param[in] rhs  Other instance
     * @return         Reference to this instance
    Xprt& operator=(const Xprt& rhs);
     */

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval `true`   Valid
     * @retval `false`  Invalid
     */
    operator bool() {
        return static_cast<bool>(pImpl);
    }

    Socket getSocket() const;

    SockAddr getRmtAddr() const noexcept;

    SockAddr getLclAddr() const;

    std::string to_string() const;

    /**
     * Returns a hash value. The value is symmetric: both endpoints will have
     * the same hash value.
     *
     * @return Hash value
     */
    size_t hash() const;

    void swap(Xprt& xprt) noexcept; // Missing "&" closes socket

    bool write(const void*        value, size_t nbytes) const;
    bool write(const bool         value) const;
    bool write(const uint8_t      value) const;
    bool write(const uint16_t     value) const;
    bool write(const uint32_t     value) const;
    bool write(const uint64_t     value) const;
    /**
     * @tparam UINT  Type of serialized, unsigned integer to hold string length
     */
    template<typename UINT = uint32_t> // Default size type
    bool write(const std::string& string) const;

    /// Flushes the output if possible.
    bool flush() const;

    bool read(void*        value, size_t nbytes) const;
    bool read(bool&        value) const;
    bool read(uint8_t&     value) const;
    bool read(uint16_t&    value) const;
    bool read(uint32_t&    value) const;
    bool read(uint64_t&    value) const;
    /**
     * @tparam UINT  Type of serialized, unsigned integer that holds string
     *               length
     */
    template<typename UINT = uint32_t>
    bool read(std::string& string) const;

    /**
     * Prepares the transport for further input in case the underlying socket
     * is not a byte-stream (e.g., UDP, SCTP).
     */
    void clear() const;

    void shutdown();

    /**
     * Returns a  object constructed from this transport. The object must have a
     * constructor that takes this transport.
     *
     * @tparam T        Transportable object
     * @return          An instance of the transportable object
     * @throw EofError  End-of-file on transport
     */
    template<class T>
    T read() {
        return T(*this);
    }
};

} // namespace

#endif /* MAIN_INET_XPRT_H_ */
