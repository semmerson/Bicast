/**
 * This file declares a protocol version message exchanged on the peer control
 * channel.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ControlMsg.h
 * @author: Steven R. Emmerson
 */

#ifndef VERSIONMSG_H_
#define VERSIONMSG_H_

#include "Serializable.h"

namespace hycast {

class VersionMsg final : public Serializable<VersionMsg> {
    unsigned version;
public:
    explicit VersionMsg(unsigned version);
    char* serialize(
            char*          buf,
            const size_t   bufLen,
            const unsigned version) const;
    size_t getSerialSize(unsigned version) const noexcept;
    unsigned getVersion() const;
    static unsigned deserialize(
            const char* const buf,
            const size_t      size,
            const unsigned    version);
};

} // namespace

#endif /* VERSIONMSG_H_ */
