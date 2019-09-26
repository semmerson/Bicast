/**
 * Class for serializing and deserializing.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Wire.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_NET_IO_WIRE_H_
#define MAIN_NET_IO_WIRE_H_

#include "Socket.h"

#include <string>
#include <memory>

namespace hycast {

/**
 * Class for serializing and deserializing.
 */
class Wire
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs from an implementation.
     *
     * @param[in] impl  The implementation
     */
    Wire(Impl* impl);

public:
    /**
     * Default Constructs.
     */
    Wire();

    /**
     * Constructs from a socket.
     *
     * @param[in] sock  Socket
     */
    Wire(Socket& sock);

    /**
     * Move constructs from a socket.
     *
     * @param[in] sock  Socket
     */
    Wire(Socket&& sock);

    /**
     * Destroys.
     */
    virtual ~Wire() noexcept;

    /**
     * Serializes a value.
     *
     * @param[in] value              Value to be serialized
     * @throws    std::system_error  I/O error occurred
     * @execptionsafety              Basic
     * @threadsafety                 Safe
     */
    void serialize(const uint8_t value);

    /**
     * Serializes a value.
     *
     * @param[in] value              Value to be serialized
     * @throws    std::system_error  I/O error occurred
     * @execptionsafety              Basic
     * @threadsafety                 Safe
     */
    void serialize(const uint16_t value);

    /**
     * Serializes a value.
     *
     * @param[in] value              Value to be serialized
     * @throws    std::system_error  I/O error occurred
     * @execptionsafety              Basic
     * @threadsafety                 Safe
     */
    void serialize(const uint32_t value);

    /**
     * Serializes a value.
     *
     * @param[in] value              Value to be serialized
     * @throws    std::system_error  I/O error occurred
     * @execptionsafety              Basic
     * @threadsafety                 Safe
     */
    void serialize(const uint64_t value);

    void serialize(const std::string string);

    void serialize(const void* data, const size_t nbytes);

    /**
     * Flushes all serializations to the output.
     */
    void flush();

    /**
     * Deserializes a value.
     *
     * @param[out]                 Deserialized value
     * @throws std::runtime_error  EOF
     * @execptionsafety            Basic
     * @threadsafety               Safe
     */
    void deserialize(uint8_t& value);

    /**
     * Deserializes a value.
     *
     * @param[out]                 Deserialized value
     * @throws std::runtime_error  EOF
     * @execptionsafety            Basic
     * @threadsafety               Safe
     */
    void deserialize(uint16_t& value);

    /**
     * Deserializes a value.
     *
     * @param[out]                 Deserialized value
     * @throws std::runtime_error  EOF
     * @execptionsafety            Basic
     * @threadsafety               Safe
     */
    void deserialize(uint32_t& value);

    /**
     * Deserializes a value.
     *
     * @param[out]                 Deserialized value
     * @throws std::runtime_error  EOF
     * @execptionsafety            Basic
     * @threadsafety               Safe
     */
    void deserialize(uint64_t& value);

    /**
     * Deserializes a string.
     *
     * @throws std::runtime_error  EOF
     * @execptionsafety            Basic
     * @threadsafety               Safe
     */
    void deserialize(std::string& string);

    /**
     * Deserializes an array of bytes.
     *
     * @param[in] data    The buffer to receive the data
     * @param[in] nbytes  The amount of data to read
     * @return            The number of bytes actually read
     */
    size_t deserialize(void* const data, const size_t nbytes);
};

} // namespace

#endif /* MAIN_NET_IO_WIRE_H_ */
