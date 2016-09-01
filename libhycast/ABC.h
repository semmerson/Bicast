/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ABC.h
 * @author: Steven R. Emmerson
 *
 * This file ...
 */

#ifndef ABC_H_
#define ABC_H_

class ABC {
protected:
    union {
        int   i;
        float f;
    } storage;
public:
    virtual ~ABC() = 0;
};

inline ABC::~ABC() {}

#endif /* ABC_H_ */
