/**
 * Byte-oriented, single stream network connection.
 *
 *        File: Stream.h
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

#ifndef MAIN_INET_STREAM_H_
#define MAIN_INET_STREAM_H_

#include <main/inet/SockAddr.h>
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

#endif /* MAIN_INET_STREAM_H_ */
