/**
 * This file declares interfaces for streams that preserve record boundaries.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: RecStream.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_NET_RECSTREAM_H_
#define MAIN_NET_RECSTREAM_H_

#include <stddef.h>
#include <sys/uio.h>

namespace hycast {

/**
 * Input record-preserving stream.
 */
class InRecStream
{
public:
    /**
     * Destroys.
     */
    virtual ~InRecStream() =default;

    /**
     * Scatter-receives a record. Waits for the record if necessary. If the
     * requested number of bytes to be read is less than the record size, then
     * the excess bytes are discarded.
     * @param[in] iovec   Scatter-read vector
     * @param[in] iovcnt  Number of elements in scatter-read vector
     * @param[in] peek    Whether or not to peek at the record. The data is
     *                    treated as unread and the next recv() or similar
     *                    function shall still return this data.
     * @retval    0       Stream is closed
     * @return            Actual number of bytes read into the buffers.
     */
    virtual size_t recv(
            const struct iovec* iovec,
            const int           iovcnt,
            const bool          peek = false) =0;

    /**
     * Receives a record. Waits for a record if necessary. If the requested
     * number of bytes to be read is less than the record size, then the excess
     * bytes are discarded unless peeking is requested.
     * @param[in] buf     Buffer into which to read the record
     * @param[in] nbytes  Maximum number of bytes to be read
     * @param[in] peek    Whether or not to peek at the record. The data is
     *                    treated as unread and the next recv() or similar
     *                    function shall still return this data.
     * @retval    0       Stream is closed
     * @return            Actual number of bytes read into the buffers.
     */
    size_t recv(
            void* const  buf,
            const size_t nbytes,
            const bool   peek = false);

    /**
     * Returns the size, in bytes, of the current record. Waits for a record if
     * necessary.
     * @retval 0  Stream is closed
     * @return Size, in bytes, of the current record
     */
    virtual size_t getSize() =0;

    /**
     * Discards the current record.
     */
    virtual void discard() =0;

    /**
     * Indicates if there's a current record.
     */
    virtual bool hasRecord() =0;
};

/**
 * Output record-preserving stream.
 */
class OutRecStream
{
public:
    /**
     * Destroys.
     */
    virtual ~OutRecStream() =default;

    /**
     * Scatter-sends a record.
     * @param[in] iovec   Scatter-write vector
     * @param[in] iovcnt  Number of elements in scatter-write vector
     * @throws std::system_error  I/O error
     * @exceptionsafety           Basic guarantee
     * @threadsafety              Compatible but not safe
     */
    virtual void send(
            const struct iovec* const iovec,
            const int                 iovcnt) =0;

    /**
     * Sends a record.
     * @param[in] buf     Data to write
     * @param[in] nbytes  Amount of data in bytes
     * @throws std::system_error  I/O error
     * @exceptionsafety           Basic guarantee
     * @threadsafety              Compatible but not safe
     */
    void send(
            const void* const buf,
            const size_t      nbytes);
};

/**
 * Input and output record-preserving stream.
 */
class InOutRecStream : public InRecStream, OutRecStream
{
    virtual ~InOutRecStream() =default;
};

} // namespace

#endif /* MAIN_NET_RECSTREAM_H_ */
