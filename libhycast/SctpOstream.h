/**
 * This file declares an output stream for an SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: SctpOstream.h
 * @author: Steven R. Emmerson
 */

#ifndef SCTPOSTREAM_H_
#define SCTPOSTREAM_H_

#include "SctpBuf.h"
#include "Socket.h"

#include <ostream>

namespace hycast {

class SctpOstream final : public std::ostream {
public:
    SctpOstream(
            Socket&        sock,
            const unsigned streamNum);
};

} // namespace

#endif /* SCTPOSTREAM_H_ */
