/**
 * Stream of bytes.
 *
 *        File: Stream.cpp
 *  Created on: May 13, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
