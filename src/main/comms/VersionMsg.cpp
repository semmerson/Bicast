/**
 * This file implements an abstract base class for control messages exchanged
 * on the peer control channel.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ControlMsg.cpp
 * @author: Steven R. Emmerson
 */

#include <comms/VersionMsg.h>
#include <netinet/in.h>

namespace hycast {

VersionMsg::VersionMsg(unsigned version)
    : version(version)
{}

size_t VersionMsg::getSerialSize(unsigned version) const noexcept
{
    return 4;
}

char* VersionMsg::serialize(
        char*          buf,
        const size_t   bufLen,
        const unsigned vers) const
{
    *reinterpret_cast<uint32_t*>(buf) = htonl(version);
    return buf + 4;
}

unsigned VersionMsg::deserialize(
        const char* const buf,
        const size_t      size,
        const unsigned    vers)
{
    return ntohl(*reinterpret_cast<const uint32_t*>(buf));
}

unsigned VersionMsg::getVersion() const
{
    return version;
}

} // namespace
