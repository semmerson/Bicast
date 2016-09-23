/**
 * This file declares an std::streambuf for an SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: SctpBuf.h
 * @author: Steven R. Emmerson
 */

#ifndef SCTPBUF_H_
#define SCTPBUF_H_

#include "Socket.h"

#include <streambuf>

namespace hycast {

class SctpBuf final : public std::streambuf {
    Socket   sock;
    unsigned streamId;
public:
    /**
     * Constructs from an SCTP socket and an SCTP stream number.
     * @param[in] sock      SCTP socket
     * @param[in] streamId  SCTP stream number
     */
    SctpBuf(
            Socket&        sock,
            const unsigned streamId);
};

} // namespace

#endif /* SCTPBUF_H_ */
