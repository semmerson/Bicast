/**
 * This file declares an abstract base class for serializing and deserializing
 * primitive types to and from an underlying, record-oriented, I/O object.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Codec.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_NET_CODEC_H_
#define MAIN_NET_CODEC_H_

#include <climits>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/uio.h>

namespace hycast {

/**
 * Abstract base class for encoding/decoding primitive objects.
 */
class Codec
{
protected:
    typedef uint16_t    StrLen;
    static const StrLen maxStrLen = UINT16_MAX;

    const size_t serialBufSize;  /// Serial buffer size in bytes
    char* const  serialBuf;      /// Serial buffer
    char*        nextSerial;     /// Next byte in buffer to access
    size_t       serialBufBytes; /// Number of bytes written to or remaining to
                                 /// be read from buffer
    struct iovec dma;            /// Vector for byte-array direct-memory-access

    void reset() noexcept;

public:
    /**
     * Constructs.
     * @param[in] maxSize  Maximum size of serial buffer in bytes
     */
    explicit Codec(const size_t maxSize);

    /**
     * Destroys.
     */
    virtual ~Codec() =0;

    static size_t getSerialSize(const size_t size) noexcept;
    static size_t getSerialSize(const bool* value);
    static size_t getSerialSize(const uint8_t* value);
    static size_t getSerialSize(const uint16_t* value);
    static size_t getSerialSize(const uint32_t* value);
    static size_t getSerialSize(const std::string& string);
}; // `Codec`

/**
 * Abstract base class for encoding primitive objects.
 */
class Encoder : public Codec
{
protected:
    /**
     * Writes to the underlying I/O object.
     * @param[in] iov     Scatter-write vector
     * @param[in] iovcnt  Size of vector
     */
    virtual void write(
            struct iovec* iov,
            const int     iovcnt) =0;

public:
    /**
     * Constructs.
     * @param[in] maxSize  Maximum size of serial buffer in bytes
     */
    explicit Encoder(const size_t maxSize);

    /**
     * Destroys.
     */
    virtual ~Encoder();

    /**
     * Serializes a boolean value into the serial buffer.
     * @param[in] value  Value to serialize
     * @return           Number of bytes written
     */
    size_t encode(const bool value);

    /**
     * Serializes an 8-bit, unsigned integer into the serial buffer.
     * @param[in] value  Value to serialize
     * @return Number of bytes written
     */
    size_t encode(const uint8_t value);

    /**
     * Serializes a 16-bit, unsigned integer into the serial buffer.
     * @param[in] value  Value to serialize
     * @return Number of bytes written
     */
    size_t encode(const uint16_t value);

    /**
     * Serializes a 32-bit, unsigned integer into the serial buffer.
     * @param[in] value  Value to serialize
     * @return Number of bytes written
     */
    size_t encode(const uint32_t value);

    /**
     * Serializes a 64-bit, unsigned integer into the serial buffer.
     * @param[in] value  Value to serialize
     * @return Number of bytes written
     */
    size_t encode(const uint64_t value);

    /**
     * Serializes a string into the serial buffer.
     * @param[in] string  String to serialize
     * @return Number of bytes written
     * @throws std::invalid_argument  String is too long
     */
    size_t encode(const std::string& string);

    /**
     * Serializes a byte-array. May be called at most once between calls to
     * write(). The array isn't serialized into the serial buffer. Instead, it's
     * location and length are saved for a subsequent scatter-write. Therefore,
     * the array must persist until the data is written.
     * @param[in] bytes Array to serialize
     * @param[in] len   Size of array in bytes
     * @return Number of bytes written (same as `len`)
     * @throws std::runtime_error  Already called
     */
    size_t encode(
            const void*  bytes,
            const size_t len);

    /**
     * Writes the serial buffer and any byte-array to the underlying I/O object.
     * Clears the serial buffer.
     */
    void flush();
};

/**
 * Abstract base class for decoding primitive objects.
 */
class Decoder : public Codec
{
protected:
    /**
     * Reads from the underlying I/O object.
     * @param[in] iov     Scatter-read vector
     * @param[in] iovcnt  Size of vector
     * @param[in] peek    Whether the bytes should remain in the I/O object
     * @return            Number of bytes actually read
     */
    virtual size_t read(
            const struct iovec* iov,
            const int           iovcnt,
            const bool          peek = false) =0;

    /**
     * Causes the underlying I/O object to discard the current message.
     */
    virtual void discard() =0;

public:
    /**
     * Constructs.
     * @param[in] maxSize  Maximum size of serial buffer in bytes
     */
    explicit Decoder(const size_t maxSize);

    /**
     * Destroys.
     */
    virtual ~Decoder();

    /**
     * Reads additional bytes from the underlying I/O object into the serial
     * buffer. Calls are cumulative: each adds to the serial buffer.
     * @param[in] nbytes  Number of additional bytes to read or 0, in which case
     *                    an attempt is made to read the maximum possible number
     *                    of bytes
     * @return            Number of bytes actually added to the serial buffer
     */
    size_t fill(size_t nbytes = 0);

    /**
     * Deserializes a boolean value. Advances the location in the serial buffer.
     */
    void decode(bool& value);

    /**
     * Deserializes an 8-bit, unsigned integer from the serial buffer. Advances
     * the location in the serial buffer.
     * @return Deserialized value
     */
    void decode(uint8_t& value);

    /**
     * Deserializes a 16-bit, unsigned integer from the serial buffer. Advances
     * the location in the serial buffer.
     */
    void decode(uint16_t& value);

    /**
     * Deserializes a 32-bit, unsigned integer from the serial buffer. Advances
     * the location in the serial buffer.
     */
    void decode(uint32_t& value);

    /**
     * Deserializes a 64-bit, unsigned integer from the serial buffer. Advances
     * the location in the serial buffer.
     */
    void decode(uint64_t& value);

    /**
     * Deserializes a string from the serial buffer.
     */
    void decode(std::string& string);

    /**
     * Deserializes a byte-array. May be called at most once between calls to
     * `read(const size_t nbytes)`. The array isn't read from the serial buffer.
     * Instead, it's read from the underlying I/O object. Doesn't advance the
     * location in the serial buffer.
     * @param[in] bytes   Destination
     * @param[in] len     Number of bytes to read
     * @return            Number of bytes actually read
     */
    size_t decode(
            void* const  bytes,
            const size_t len);

    /**
     * Returns the number of bytes yet to be read from the message.
     * @return Number of bytes yet to be read
     */
    size_t numRemainingBytes();

    /**
     * Clears the current message. Sets the location in the serial buffer to its
     * beginning.
     */
    void clear();

    /**
     * Indicates if this instance has a record.
     * @return `true` iff this instance has a record
     */
    virtual bool hasRecord() =0;

    /**
     * Returns the size of the message in bytes -- including any read bytes.
     * @return Size of message in bytes
     */
    virtual size_t getSize() =0;

    /**
     * Indicates if this instance is considered equal to another.
     * @param[in] that  Other instance
     * @retval `true`   Instances are equal
     * @retval `false`  Instances are not equal
     */
    virtual bool operator==(const Decoder& that) const noexcept;
}; // `Decoder`

class MemEncoder final : public Encoder
{
    char*  buf;   // Byte buffer
    size_t size;  // Number of bytes written to buffer

public:
    MemEncoder(
            char* const  buf,
            const size_t maxSize);

    void write(
            struct iovec* iov,
            const int     iovcnt);
};

class MemDecoder final : public Decoder
{
    const char*  memBuf;   // Byte buffer
    size_t       memRead;  // Number of bytes read from buffer

    void discard()
    {}

public:
    /**
     * Constructs.
     * @param[in] msg     Message to be decoded
     * @param[in] msgLen  Size of message in bytes
     */
    MemDecoder(
            const char* const  msg,
            const size_t       msgLen);

    bool hasRecord();

    /**
     * Returns the size of the message in bytes, which is the same value that
     * was given to the constructor.
     * @return Size of message in bytes
     */
    size_t getSize();

    size_t read(
            const struct iovec* iov,
            const int           iovcnt,
            const bool          peek = false);

    bool operator==(const MemDecoder& that) const noexcept;

    bool operator==(const Decoder& that) const noexcept;
};

} // namespace

#endif /* MAIN_NET_CODEC_H_ */
