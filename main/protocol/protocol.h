/**
 * Declarations related to the protocol.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: protocol.h
 *  Created on: Feb 22, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROTOCOL_PROTOCOL_H_
#define MAIN_PROTOCOL_PROTOCOL_H_

#include <memory>

namespace hycast {

typedef enum {
    PROD_INFO_NOTICE = 1,
    DATA_SEG_NOTICE,
    PROD_INFO_REQUEST,
    DATA_SEG_REQUEST,
    PROD_INFO,
    DATA_SEG,
    PATH_TO_SRC,
    NO_PATH_TO_SRC
} MsgId;

} // namespace

#endif /* MAIN_PROTOCOL_PROTOCOL_H_ */
