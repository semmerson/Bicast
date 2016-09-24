/**
 * This file defines types common to the Hycast package.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: HycastTypes.h
 * @author: Steven R. Emmerson
 */

#ifndef HYCASTTYPES_H_
#define HYCASTTYPES_H_

#include <cstdint>

namespace hycast {

typedef uint_fast32_t ProdIndex;
typedef uint_fast32_t ProdSize;
typedef uint_fast32_t ChunkIndex;
typedef uint_fast16_t ChunkSize;

static const ProdIndex  prodIndexMax  = UINT32_MAX;
static const ProdSize   prodSizeMax   = UINT32_MAX;
static const ChunkIndex chunkIndexMax = UINT32_MAX;
static const ChunkSize  chunkSizeMax  = UINT16_MAX;

}

#endif /* HYCASTTYPES_H_ */
