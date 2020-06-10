/**
 * Interface for objects that can be serialized and deserialized.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Serializable.h
 *  Created on: May 3, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_INET_SERIALIZABLE_H_
#define MAIN_INET_SERIALIZABLE_H_

#include "Rpc.h"

namespace hycast {

class Serializable
{
public:
    virtual ~Serializable() noexcept
    {}
};

} // namespace

#endif /* MAIN_INET_SERIALIZABLE_H_ */
