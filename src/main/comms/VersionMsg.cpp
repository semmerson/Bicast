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
    return Codec::getSerialSize(&version);
}

void VersionMsg::serialize(
        Encoder&       encoder,
        const unsigned version)
{
    encoder.encode(this->version);
}

VersionMsg VersionMsg::deserialize(
        Decoder&        decoder,
        const unsigned  version)
{
    unsigned vers;
    decoder.decode(vers);
    return VersionMsg(vers);
}

unsigned VersionMsg::getVersion() const
{
    return version;
}

} // namespace
