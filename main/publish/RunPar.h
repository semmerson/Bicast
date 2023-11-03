/**
 * Runtime parameters for the publish(1) program.
 *
 *        File: publish.cpp
 *  Created on: Aug 13, 2020
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef MAIN_PUBLISH_RUNPAR_H_
#define MAIN_PUBLISH_RUNPAR_H_

#include "CommonTypes.h"

namespace bicast {

/// Namespace for creating and accessing runtime parameters for the publish(1) program.
namespace RunPar {

extern String progName;

/**
 * Sets runtime parameters from the command-line.
 *
 * @param[in] argc               Number of command-line arguments
 * @param[in] argv               Command-line arguments
 * @throw std::invalid_argument  Invalid option, option argument, or operand
 * @throw std::logic_error       Too many or too few operands
 */
void init(
        int          argc,
        char* const* argv);

} // `RunPar` namespace

} // `bicast` namespace

#endif /* MAIN_PUBLISH_RUNPAR_H_ */
