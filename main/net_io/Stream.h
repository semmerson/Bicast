/**
 * Byte-oriented, single stream network connection.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Stream.h
 *  Created on: May 13, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_NET_IO_STREAM_H_
#define MAIN_NET_IO_STREAM_H_

#include "SockAddr.h"

#include <memory>

namespace hycast {

class Stream
{
public:
    class                 Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    Stream(Impl* const impl);

public:
    static ServerStream getServer(const SockAddr& srvrAddr);

    virtual ~Stream() noexcept =0;
};

/******************************************************************************/

class ServerStream final : public Stream
{
public:
    Stream accept();

    ~ServerStream();
};

} // namespace

#endif /* MAIN_NET_IO_STREAM_H_ */
