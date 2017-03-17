/**
 * This file declares a protocol version message exchanged on the peer control
 * channel.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ControlMsg.h
 * @author: Steven R. Emmerson
 */

#ifndef VERSIONMSG_H_
#define VERSIONMSG_H_

#include "Codec.h"
#include "Serializable.h"

namespace hycast {

class VersionMsg final : public Serializable<VersionMsg>
{
    unsigned version;

public:
    explicit VersionMsg(unsigned version);

    /**
     * Serializes this instance to an encoder.
     * @param[in] encoder  Encoder
     * @param[in] version  Protocol version
     * @return Number of bytes written
     * @exceptionsafety Basic Guarantee
     * @threadsafety    Safe
     */
    size_t serialize(
            Encoder&       encoder,
            const unsigned version) const;

    size_t getSerialSize(unsigned version) const noexcept;

    unsigned getVersion() const;

    /**
     * Returns an instance corresponding to the serialized representation in a
     * decoder.
     * @param[in] decoder    Decoder
     * @param[in] version    Protocol version
     * @exceptionsafety Basic
     * @threadsafety    Compatible but not thread-safe
     */
    static VersionMsg deserialize(
            Decoder&        decoder,
            const unsigned  version);
};

} // namespace

#endif /* VERSIONMSG_H_ */
