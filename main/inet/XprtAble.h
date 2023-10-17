/**
 * This file declares a socket-based transport mechanism that is independent of
 * the socket's underlying protocol (TCP, UCP, etc.).
 *
 *  @file:  Xprt.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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

#ifndef MAIN_INET_XPRTABLE_H_
#define MAIN_INET_XPRTABLE_H_

namespace bicast {

class Xprt; ///< Forward declaration

/**
 * Interface for a transportable object (i.e., one that can be serialized and deserialized over
 * a socket).
 */
class XprtAble
{
public:
    virtual ~XprtAble() {};

    /**
     * Writes itself to a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    virtual bool write(Xprt& xprt) const =0;

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    virtual bool read(Xprt& xprt) =0;
};

} // namespace

#endif /* MAIN_INET_XPRTABLE_H_ */
