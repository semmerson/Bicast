/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Exception.h
 * @author: Steven R. Emmerson
 *
 * This file defines the API for exceptions for the Hycast package.
 */

#ifndef EXCEPTION_H_
#define EXCEPTION_H_

namespace hycast {

class Exception {
private:
    const char* file;
    const int   line;
public:
    Exception(
            const char* file,
            const int   line);
    virtual ~Exception() {};        // definition must exist
};

}

#endif /* EXCEPTION_H_ */
