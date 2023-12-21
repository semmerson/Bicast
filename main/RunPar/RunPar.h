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

#include "BicastProto.h"
#include "CommonTypes.h"
#include "logging.h"
#include "SockAddr.h"

namespace bicast {

/// Namespace for accessing runtime parameters.
namespace RunPar {

/// Command-line parameters common to both publisher and subscriber:
extern bool        initializeOnly;    ///< Initialize only: do not execute
extern SysDuration heartbeatInterval; ///< Time interval between heartbeat packets. <0 => no heartbeat
extern String      logLevel;          ///< Logging level
extern int         maxNumPeers;       ///< Maximum number of neighboring peers
extern unsigned    maxOpenProds;      ///< Maximum number of product-files to keep open
extern SockAddr    p2pSrvrAddr;       ///< Address of the P2P server
extern int         p2pSrvrQSize;      ///< Maximum number of pending P2P connections
extern SysDuration peerEvalInterval;  ///< Time interval for evaluating peer performance
extern String      progName;          ///< Program name
extern SockAddr    pubSrvrAddr;       ///< Address of publisher's server (not its P2P server)
extern int         trackerCap;        ///< Maximum number of potential P2P servers to track & exchange
extern bool        natTraversal;      ///< Whether or not to attempt NAT traversal

/// Publisher-specific command-line parameters:
extern String      pubRoot;           ///< Pathname of the root-directory
extern int         pubSrvrQSize;      ///< Maximum number of pending connections to publisher's server
extern String      feedName;          ///< Name of the feed
extern SegSize     maxSegSize;        ///< Size of a canonical data-segment
extern SockAddr    mcastDstAddr;      ///< Multicast group destination address
extern InetAddr    mcastSrcAddr;      ///< Multicast group source address
extern SysDuration prodKeepTime;      ///< Amount of time to keep products before deleting them

/// Subscriber-specific command-line parameters:
extern String      subRoot;           ///< Pathname of the root-directory
extern InetAddr    mcastIface;        ///< Internet address of interface for receiving multicast
extern String      disposeConfig;     ///< Pathname of configuration-file for disposition of products
extern SysDuration p2pTimeout;        ///< Timeout for connecting to remote P2P servers
extern SysDuration retryInterval;     ///< Time to wait before re-connecting to publisher
extern in_port_t   natPort;           ///< Port number of NAT device for P2P server. 0 => not NATed

/**
 * Initializes from the command-line. Must be called first.
 * @param[in] argc  Number of command-line arguments
 * @param[in] argv  Command-line arguments
 */
void init(
        const int          argc,
        const char* const* argv);

/**
 * Sets the common runtime parameters from a YAML configuration-file.
 * @param[in] pathname  Pathname of the configuration-file
 * @throw RuntimeError  Parser failure
 */
void setFromYaml(const String& pathname);

/**
 * Vets the common runtime parameters.
 * @throw InvalidArgument  A runtime parameter is invalid
 */
void vet();

#define RUNPAR_COMMON_OPTIONS_STRING ":b:e:hl:Nn:o:p:q:t:" // Must go first
#define RUNPAR_COMMON_OPTIONS_CASES(usage) \
            case 'h': { \
                usage(); \
                exit(0); \
            } \
            \
            case 'b': { \
                int seconds; \
                if (::sscanf(optarg, "%d", &seconds) != 1) \
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) + \
                            "\" option argument"); \
                RunPar::heartbeatInterval = SysDuration(std::chrono::seconds(seconds)); \
                break; \
            } \
            case 'e': { \
                int evalTime; \
                if (::sscanf(optarg, "%d", &evalTime) != 1) \
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) + \
                            "\" option argument"); \
                RunPar::peerEvalInterval = SysDuration(std::chrono::seconds(evalTime)); \
                break; \
            } \
            case 'l': { \
                log_setLevel(optarg); \
                RunPar::logLevel = optarg; \
                break; \
            } \
            case 'N': { \
                RunPar::natTraversal = true; \
            } \
            case 'n': { \
                int maxPeers; \
                if (::sscanf(optarg, "%d", &maxPeers) != 1) \
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) + \
                            "\" option argument"); \
                RunPar::maxNumPeers = maxPeers; \
                break; \
            } \
            case 'o': { \
                if (::sscanf(optarg, "%u", &RunPar::maxOpenProds) != 1) \
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) + \
                            "\" option argument"); \
                break; \
            } \
            case 'p': { \
                RunPar::p2pSrvrAddr = SockAddr(optarg); \
                break; \
            } \
            case 'q': { \
                int size; \
                if (::sscanf(optarg, "%d", &size) != 1) \
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) + \
                            "\" option argument"); \
                RunPar::p2pSrvrQSize = size; \
                break; \
            } \
            case 't': { \
                int trackerCap; \
                if (::sscanf(optarg, "%d", &trackerCap) != 1) \
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) + \
                            "\" option argument"); \
                RunPar::trackerCap = trackerCap; \
                break; \
            } \
            case ':': { \
                throw INVALID_ARGUMENT(String("Option \"-") + static_cast<char>(optopt) + \
                        "\" is missing an argument"); \
            } \
            default : { \
                throw INVALID_ARGUMENT(String("Unknown \"-") + static_cast<char>(optopt) + \
                        "\" option"); \
            }

} // `RunPar` namespace

} // `bicast` namespace

#endif /* MAIN_PUBLISH_RUNPAR_H_ */
