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

/// A class used to transport objects
class Xprt
{
public:
    using PduId    = uint32_t; ///< Type of product data unit identifier

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

#if 0
    /**
     * Copy constructs.
     *
     * @param[in] xprt  Other instance
     */
    Xprt(const Xprt& xprt);

    /**
     * Destroys.
     */
    ~Xprt() noexcept;

    /**
     * Copy Assigns.
     *
     * @param[in] rhs  Other instance
     * @return         Reference to this instance
     */
    Xprt& operator=(const Xprt& rhs);
#endif

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval true     Valid
     * @retval false    Invalid
     */
    operator bool() {
        return static_cast<bool>(pImpl);
    }

    /**
     * Returns the underlying socket.
     * @return The underlying socket
     */
    Socket getSocket() const;

    /**
     * Returns the socket address of the remote endpoint.
     * @return The socket address of the remote endpoint
     */
    SockAddr getRmtAddr() const noexcept;

    /**
     * Returns the socket address of the local endpoint.
     * @return The socket address of the local endpoint
     */
    SockAddr getLclAddr() const;

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    std::string to_string() const;

    /**
     * Returns a hash value. The value is symmetric: both endpoints will have
     * the same hash value.
     *
     * @return Hash value
     */
    size_t hash() const;

    /**
     * Swaps this instance with another.
     * @param[in,out] xprt  The other instance
     */
    void swap(Xprt& xprt) noexcept; // Missing "&" closes socket

    /**
     * Writes bytes.
     * @param[in] value    Bytes to write
     * @param[in] nbytes   Number of bytes to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const void*        value, size_t nbytes) const;
    /**
     * Writes a boolean value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const bool         value) const;
    /**
     * Writes an unsigned, 8-bit value value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const uint8_t      value) const;
    /**
     * Writes an unsigned, 16-bit value value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const uint16_t     value) const;
    /**
     * Writes an unsigned, 32-bit value value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const uint32_t     value) const;
    /**
     * Writes an unsigned, 64-bit value value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const uint64_t     value) const;
    /**
     * @tparam UINT  Type of serialized, unsigned integer to hold string length
     */
    template<typename UINT = uint32_t> // Default size type
    bool write(const std::string& string) const;

    /// Flushes the output if possible.
    bool flush() const;

    /**
     * Reads bytes.
     * @param[out] value    The buffer to be set
     * @param[in]  nbytes   The number of bytes to read into the buffer
     * @retval     true     Success
     * @retval     false    Connection lost
     */
    bool read(void*        value, size_t nbytes) const;
    /**
     * Reads a boolean value.
     * @param[out] value    The value to be set
     * @retval     true     Success
     * @retval     false    Connection lost
     */
    bool read(bool&        value) const;
    /**
     * Reads an unsigned, 8-bit value.
     * @param[out] value    The value to be set
     * @retval     true     Success
     * @retval     false    Connection lost
     */
    bool read(uint8_t&     value) const;
    /**
     * Reads an unsigned, 16-bit value.
     * @param[out] value    The value to be set
     * @retval     true     Success
     * @retval     false    Connection lost
     */
    bool read(uint16_t&    value) const;
    /**
     * Reads an unsigned, 32-bit value.
     * @param[out] value    The value to be set
     * @retval     true     Success
     * @retval     false    Connection lost
     */
    bool read(uint32_t&    value) const;
    /**
     * Reads an unsigned, 64-bit value.
     * @param[out] value    The value to be set
     * @retval     true     Success
     * @retval     false    Connection lost
     */
    bool read(uint64_t&    value) const;
    /**
     * @tparam UINT  Type of serialized, unsigned integer that holds string
     *               length
     * @retval     true     Success
     * @retval     false    Connection lost
     */
    template<typename UINT = uint32_t>
    bool read(std::string& string) const;

    /**
     * Prepares the transport for further input in case the underlying socket
     * is not a byte-stream (e.g., UDP, SCTP).
     */
    void clear() const;

    /**
     * Shuts down the connection. Doesn't close the socket, however.
     */
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
