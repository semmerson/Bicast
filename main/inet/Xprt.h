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

#include "CommonTypes.h"
#include "Socket.h"

#include <memory>

namespace bicast {

/// A class used to transport objects
class Xprt
{
public:
    class Impl; // Implementation

private:
    std::shared_ptr<Impl> pImpl;

public:
    Xprt() =default;

    /**
     * Constructs from an underlying socket.
     *
     * @param[in] sock  Socket
     */
    Xprt(const Socket sock);

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
    bool write(const void*       value, size_t nbytes) const;
    /**
     * Writes a boolean value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const bool        value) const;
    /**
     * Writes an unsigned, 8-bit value value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const uint8_t     value) const;
    /**
     * Writes an signed, 8-bit value value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const int8_t      value) const;
    /**
     * Writes an unsigned, 16-bit value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const uint16_t    value) const;
    /**
     * Writes an signed, 16-bit value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const int16_t     value) const;
    /**
     * Writes an unsigned, 32-bit value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const uint32_t    value) const;
    /**
     * Writes an signed, 32-bit value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const int32_t     value) const;
    /**
     * Writes an unsigned, 64-bit value value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const uint64_t    value) const;
    /**
     * Writes an signed, 64-bit value value.
     * @param[in] value    Value to write
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const int64_t     value) const;
    /**
     * @tparam UINT  Type of serialized, unsigned integer to hold string length
     */
    template<typename UINT = uint32_t> // Default size type
    bool write(const std::string& string) const;

    /**
     * Writes a system time-point.
     * @param[in] time     The system time-point to be written
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(const SysTimePoint& time) const;

    /**
     * Writes a system time-duration.
     * @param[in] duration  The system time-duration to be written
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    bool write(const SysDuration& duration) const;

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
     * Reads an signed, 16-bit value.
     * @param[out] value    The value to be set
     * @retval     true     Success
     * @retval     false    Connection lost
     */
    bool read(int16_t&     value) const;
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
     * Reads a system time-point.
     * @param[in] time     The system time-point to be set
     * @retval    true     Success. `time` is set.
     * @retval    false    Lost connection
     */
    bool read(SysTimePoint& time) const;

    /**
     * Reads a system time-duration.
     * @param[in] duration  The system time-duration to be set
     * @retval    true      Success. `time` is set.
     * @retval    false     Lost connection
     */
    bool read(SysDuration& duration) const;

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
