/**
 * Interface to object that sends and receives objects.
 *
 *        File: Channel.h
 *  Created on: May 5, 2019
 *      Author: Steven R. EmmersonS
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

#ifndef MAIN_INET_CHANNEL_H_
#define MAIN_INET_CHANNEL_H_

#include <main/inet/IoVec.h>
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

#endif /* MAIN_INET_CHANNEL_H_ */
