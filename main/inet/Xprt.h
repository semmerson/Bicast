/**
 * This file declares a socket-based transport mechanism that is independent of
 * the socket's underlying protocol (TCP, UCP, etc.).
 *
 *  @file:  Xprt.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#ifndef MAIN_INET_XPRT_H_
#define MAIN_INET_XPRT_H_

#include <Socket.h>

#include <memory>

namespace hycast {

class Xprt
{
public:
    using PduId    = uint32_t;
    using Dispatch = std::function<bool(PduId, Xprt)>;

    class Impl; // Implementation

private:
    std::shared_ptr<Impl> pImpl;

public:
    Xprt() =default;

    /**
     * Constructs.
     *
     * @param[in] sock      Socket
     */
    Xprt(Socket& sock);

    /**
     * Indicates if this instance is value (i.e., wasn't default constructed).
     *
     * @retval `true`   Valid
     * @retval `false`  Invalid
     */
    operator bool() {
        return static_cast<bool>(pImpl);
    }

    SockAddr getRmtAddr() const;

    std::string to_string() const;

    /**
     * Sends a PDU ID as a PDU to the remote host.
     *
     * @param[in] pduId    PDU identifier
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool send(const PduId pduId);

    /**
     * Sends a boolean as a PDU to the remote host.
     *
     * @param[in] pduId    PDU identifier
     * @param[in] value    Boolean to be transported
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool send(const PduId pduId, const bool value);

    /**
     * Sends an object as a PDU to the remote host.
     *
     * @param[in] pduId    PDU identifier
     * @param[in] obj      Object to be transported
     * @retval    `true`   Success
     * @retval    `false`  Connection lost
     */
    bool send(const PduId pduId, const XprtAble& obj);

    /**
     * Receives and processes the next, incoming PDU. Calls the dispatch
     * function given to the constructor.
     *
     * @param[in] dispatch      Dispatch function for incoming PDU-s
     * @retval `true`           Success
     * @retval `false`          Connection lost
     */
    bool recv(Dispatch& dispatch);

    bool write(const void*        value, size_t nbytes);
    bool write(const bool         value);
    bool write(const uint8_t      value);
    bool write(const uint16_t     value);
    bool write(const uint32_t     value);
    bool write(const uint64_t     value);
    bool write(const std::string& value);

    bool read(void*        value, size_t nbytes);
    bool read(bool&        value);
    bool read(uint8_t&     value);
    bool read(uint16_t&    value);
    bool read(uint32_t&    value);
    bool read(uint64_t&    value);
    bool read(std::string& value);

    void shutdown();
};

} // namespace

#endif /* MAIN_INET_XPRT_H_ */
