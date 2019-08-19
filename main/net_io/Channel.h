/**
 * Interface to object that sends and receives objects.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Channel.h
 *  Created on: May 5, 2019
 *      Author: Steven R. EmmersonS
 */

#ifndef MAIN_NET_IO_CHANNEL_H_
#define MAIN_NET_IO_CHANNEL_H_

#include "IoVec.h"

#include <stdint.h>

namespace hycast {

class Channel
{

protected:

public:
    ~Channel(const int numStreams) noexcept =0;

    void send(const IoVec& ioVec) =0;

    size_t remaining() =0;

    void recv(uint16_t& value) =0;

    void recv(
            const void*  data,
            const size_t nbytes) =0;
};

} // namespace

#endif /* MAIN_NET_IO_CHANNEL_H_ */
