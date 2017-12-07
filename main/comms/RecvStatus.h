/**
 * This file declares the status associated with receiving something.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: RecvStatus.h
 *  Created on: Nov 21, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_RECVSTATUS_H_
#define MAIN_COMMS_RECVSTATUS_H_

namespace hycast {

/**
 * Status of the reception of something.
 */
class RecvStatus
{
    unsigned              status;
    static const unsigned IS_COMPLETE = 1;
    static const unsigned IS_NEW = 2;
    static const unsigned IS_NEEDED = 4;
public:
    inline RecvStatus() : status{0}  {}
    inline RecvStatus& setNew()      { status |= IS_NEW;      return *this; }
    inline RecvStatus& setComplete() { status |= IS_COMPLETE; return *this; }
    inline RecvStatus& setNeeded()   { status |= IS_NEEDED;   return *this; }
    inline bool isNew()       const  { return status & IS_NEW; }
    inline bool isComplete()  const  { return status & IS_COMPLETE; }
    inline bool isNeeded()    const  { return status & IS_NEEDED; }
};

} // namespace

#endif /* MAIN_COMMS_RECVSTATUS_H_ */
