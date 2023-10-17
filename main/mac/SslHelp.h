/**
 * SSL helper functions.
 *
 *        File: SslHelp.h
 *  Created on: Mar 29, 2021
 *      Author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HYCAST_MAIN_MAC_SSLHELP_H
#define HYCAST_MAIN_MAC_SSLHELP_H

#include <queue>
#include <string>

namespace SslHelp {

using OpenSslErrCode = unsigned long; ///< Type of an OpenSSL error code
using CodeQ          = std::queue<OpenSslErrCode>; ///< Queue of OpenSSL error codes

/**
 * Initializes the OpenSSL pseudo-random number generator (PRNG).
 *
 * @param[in] numBytes         Number of bytes from "/dev/random" to initialize
 *                             the PRNG with
 * @throws std::system_error   Couldn't open "/dev/random"
 * @throws std::system_error   `read(2)` failure
 * @throws std::runtime_error  `RAND_bytes()` failure
 */
void initRand(const int numBytes);

/**
 * Throws a queue of OpenSSL errors as a nested exception. If the queue is
 * empty, then simply returns. Recursive.
 *
 * @param[in,out] codeQ         Queue of OpenSSL error codes. Will be empty
 *                              on return.
 * @throw std::runtime_error    Earliest OpenSSL error
 * @throw std::nested_exception Nested OpenSSL runtime exceptions
 */
void throwExcept(CodeQ& codeQ);

/**
 * Throws an OpenSSL error. If a current OpenSSL error exists, then it is
 * thrown as a nested exception; otherwise, a regular exception is thrown.
 *
 * @param msg                     Top-level (non-OpenSSL) message
 * @throw std::runtime_exception  Regular or nested exception
 */
void throwOpenSslError(const std::string& msg);

} // Namespace

#endif /* HYCAST_MAIN_MAC_SSLHELP_H */
