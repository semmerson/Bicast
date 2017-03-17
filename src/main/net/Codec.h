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

    static size_t getSerialSize(const size_t size);
    static size_t getSerialSize(const uint16_t* value);
    static size_t getSerialSize(const uint32_t* value);
    static size_t getSerialSize(const std::string& string);
};

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
            const struct iovec* iov,
            const int           iovcnt) =0;

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
     * Deserializes a 16-bit, unsigned integer from the serial buffer. Advances
     * the location in the serial buffer.
     * @return Deserialized value
     */
    void decode(uint16_t& value);

    /**
     * Deserializes a 32-bit, unsigned integer from the serial buffer. Advances
     * the location in the serial buffer.
     * @return Deserialized value
     */
    void decode(uint32_t& value);

    /**
     * Deserializes a string from the serial buffer.
     * @return Deserialized value
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
     * Clears the current message. Sets the location in the serial buffer to its
     * beginning.
     */
    void clear();

    /**
     * Indicates if this instance has a record.
     * @return `true` iff this instance has a record
     */
    virtual bool hasRecord() =0;
};

class MemEncoder final : public Encoder
{
    char*  buf;   // Byte buffer
    size_t size;  // Number of bytes written to buffer

public:
    MemEncoder(
            char* const  buf,
            const size_t maxSize);

    void write(
            const struct iovec* iov,
            const int           iovcnt);
};

class MemDecoder final : public Decoder
{
    const char*  memBuf;   // Byte buffer
    size_t       memRead;  // Number of bytes read from buffer

    void discard()
    {}

public:
    MemDecoder(
            const char* const  buf,
            const size_t       size);

    bool hasRecord();

    size_t getSize();

    size_t read(
            const struct iovec* iov,
            const int           iovcnt,
            const bool          peek = false);
};

} // namespace

#endif /* MAIN_NET_CODEC_H_ */
