/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYIING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket.h
 * @author: Steven R. Emmerson
 *
 * This file declares a RAII object for a socket.
 */

#ifndef SOCKET_H_
#define SOCKET_H_

#include <memory>
#include <string>

namespace hycast {

class Socket final {
    int                  sock;
    std::shared_ptr<int> sptr;
    void copy_assign(const Socket& that);
public:
                Socket();
                Socket(const int sock);
                Socket(const Socket& socket);
                Socket(const Socket&& socket);
                ~Socket();
    Socket&     operator=(const Socket& socket);
    Socket&     operator=(const Socket&& socket);
    bool        operator==(const Socket& that) const {
                    return sock == that.sock;
                }
    std::string to_string() const {
                    return std::to_string(sock);
                }
};

} // namespace

#endif
