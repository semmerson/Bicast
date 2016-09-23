/**
 * This file declares a serializer/deserializer.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Serdes.h
 * @author: Steven R. Emmerson
 */

#ifndef SERDES_H_
#define SERDES_H_

#include "PeerConnection.h"
#include "ProdInfo.h"

#include <cstdint>
#include <sys/uio.h>

namespace hycast {

class Serdes {
    Serdes();
    Serdes(Serdes& serdes);
    Serdes& operator=(const Serdes& rhs);

    static const int    PROD_INFO_IOVCNT = 4;
    static const size_t PROD_INFO_FIXED_LEN = sizeof(ProdInfo::index) +
            sizeof(ProdInfo::size) + sizeof(ProdInfo::chunkSize);
    PeerConnection conn;
public:
    Serdes(PeerConnection& conn)
        : conn(conn) {};
    void serialize(
            Socket&         sock,
            const unsigned  streamId,
            const ProdInfo& prodInfo);
    void deserialize(
            const ProdInfo& prodInfo,
            const size_t    nbytes);
};

} // namespace

#endif /* SERDES_H_ */
