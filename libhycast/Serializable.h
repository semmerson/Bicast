/**
 * This file defines an interface for classes that can be serialized.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Serializable.h
 * @author: Steven R. Emmerson
 */

#ifndef SERIALIZABLE_H_
#define SERIALIZABLE_H_

#include "Socket.h"

namespace hycast {

class Serializable {
    Serializable(const Serializable& that);
    Serializable& operator=(const Serializable& rhs);
public:
    Serializable() {};
    virtual ~Serializable() {};
    /**
     * Serializes this instance to an SCTP socket.
     * @param[in] sock      SCTP socket
     * @param[in] streamId  SCTP stream number to use
     * @param[in] version   Serialization version
     */
    virtual void serialize(
            Socket&        sock,
            const unsigned streamId,
            const unsigned version) const =0;
};

} // namespace

#endif /* SERIALIZABLE_H_ */
