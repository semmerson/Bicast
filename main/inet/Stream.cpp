/**
 * 
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Stream.cpp
 *  Created on: May 13, 2019
 *      Author: Steven R. Emmerson
 */

#include <main/inet/Stream.h>
#include "config.h"


namespace hycast {

class Stream::Impl {
public:
    virtual ~Impl() =0;
};

/******************************************************************************/

class TcpStream final : public Stream::Impl
{
private:

public:
    TcpStream()
    {}

    ~TcpStream()
    {}
};

/******************************************************************************/

Stream::~Stream()
{}

Stream::Stream(Impl* const impl)
    : pImpl{impl}
{}

/******************************************************************************/

ServerStream Stream::getServer(const SockAddr& srvrAddr)
{
    return ServerStream(new TcpServer());
}

} // namespace
