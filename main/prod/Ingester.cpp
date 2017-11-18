/**
 * This file implements the abstract base class for an ingester of
 * data-products. An ingester provides a sequence of products for a source-node
 * to transmit.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Ingester.cpp
 *  Created on: Oct 30, 2017
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "Ingester.h"

namespace hycast {

Ingester::~Ingester()
{}

} // namespace
