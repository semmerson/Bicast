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

#include "BicastProto.h"
#include "RunPar.h"
#include "error.h"
#include "FileUtil.h"
#include "logging.h"
#include "Parser.h"
#include "SockAddr.h"
#include "Socket.h"

#include <cstring>
#include <exception>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

namespace bicast {
namespace RunPar {

using namespace bicast;

/// Runtime parameters common to both publisher and subscriber:
bool        initializeOnly;    ///< Initialize only: do not execute
SysDuration heartbeatInterval; ///< Time interval between heartbeat packets. <0 => no heartbeat
String      logLevel;          ///< Initial logging level
int         maxNumPeers;       ///< Maximum number of neighboring peers
unsigned    maxOpenProds;      ///< Maximum number of product-files to keep open
SockAddr    p2pSrvrAddr;       ///< Address of the P2P server
int         p2pSrvrQSize;      ///< Maximum number of pending P2P connections
SysDuration peerEvalInterval;  ///< Time interval for evaluating peer performance
String      progName;          ///< Program name
SockAddr    pubSrvrAddr;       ///< Address of publisher's server (not its P2P server)
int         trackerCap;        ///< Maximum number of potential P2P servers to track & exchange

/// Publisher-specific runtime parameters:
String      pubRoot;           ///< Pathname of the root-directory
int         pubSrvrQSize;      ///< Maximum number of pending connections to publisher's server
String      feedName;          ///< Name of the feed
SegSize     maxSegSize;        ///< Size of a canonical data-segment
SockAddr    mcastDstAddr;      ///< Multicast group destination address
InetAddr    mcastSrcAddr;      ///< Multicast group source address
SysDuration prodKeepTime;      ///< Amount of time to keep products before deleting them

/// Subscriber-specific runtime parameters:
String      subRoot;           ///< Pathname of the root-directory
InetAddr    mcastIface;        ///< Internet address of interface for receiving multicast
String      disposeConfig;     ///< Pathname of configuration-file for disposition of products
SysDuration p2pTimeout;        ///< Timeout for connecting to remote P2P servers
SysDuration retryInterval;     ///< Time to wait before re-connecting to publisher

void init(
        const int          argc,
        const char* const* argv)
{
    initializeOnly = false;
    pubRoot = String("./pubRoot");
    subRoot = String("./subRoot");
    heartbeatInterval = SysDuration(std::chrono::seconds(30));
    logLevel = "NOTE";
    log_setLevel(logLevel);
    maxNumPeers = 8;
    maxOpenProds = ::sysconf(_SC_OPEN_MAX)/2;
    p2pSrvrAddr = SockAddr();
    p2pSrvrQSize = 8;
    peerEvalInterval = SysDuration(std::chrono::minutes(5));
    progName = FileUtil::filename(argv[0]);
    pubSrvrAddr = SockAddr();
    trackerCap = 1000;
}

void setFromYaml(const String& pathname)
{
    auto node0 = YAML::LoadFile(pathname);

    try {
        auto node1 = node0["logLevel"];
        if (node1) {
            logLevel = node1.as<String>();
            log_setLevel(logLevel);
        }
        Parser::tryDecode<decltype(trackerCap)>(node0, "trackerCap", trackerCap);

        node1 = node0["peer2Peer"];
        if (node1) {
            auto node2 = node1["server"];
            if (node2) {
                auto node3 = node2["p2pAddr"];
                if (node3)
                    p2pSrvrAddr = SockAddr(node3.as<String>());
                Parser::tryDecode<decltype(p2pSrvrQSize)>(node2, "maxPending", p2pSrvrQSize);
            }

            Parser::tryDecode<decltype(maxNumPeers)>(node1, "maxPeers", maxNumPeers);

            int seconds;
            Parser::tryDecode<decltype(seconds)>(node1, "evalTime", seconds);
            peerEvalInterval = SysDuration(std::chrono::seconds(seconds));
            Parser::tryDecode<decltype(seconds)>(node1, "heartbeatInterval", seconds);
            heartbeatInterval = std::chrono::seconds(seconds);
        }

        node1 = node0["repository"];
        if (node1) {
            Parser::tryDecode<decltype(maxOpenProds)>(node1, "maxOpenFiles", maxOpenProds);
        }
    } // YAML file loaded
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse YAML file \"" + pathname + "\""));
    }
}

void vet()
{
    if (RunPar::heartbeatInterval.count() == 0)
        throw INVALID_ARGUMENT("Heartbeat interval cannot be zero");

    if (peerEvalInterval.count() <= 0)
        throw INVALID_ARGUMENT("Peer performance evaluation-interval is not positive");

    if (trackerCap <= 0)
        throw INVALID_ARGUMENT("Tracker size is not positive");

    if (maxNumPeers <= 0)
        throw INVALID_ARGUMENT("Maximum number of peers is not positive");

    if (maxOpenProds <= 0)
        throw INVALID_ARGUMENT("Maximum number of open repository files is not positive");
    if (maxOpenProds > sysconf(_SC_OPEN_MAX))
        throw INVALID_ARGUMENT("Maximum number of open repository files is "
                "greater than system maximum, " + std::to_string(sysconf(_SC_OPEN_MAX)));

    if (prodKeepTime.count() <= 0)
        throw INVALID_ARGUMENT("How long to keep product files is not positive");

    if (p2pSrvrQSize <= 0)
        throw INVALID_ARGUMENT("Size of P2P server-queue is not positive");

    if (pubRoot.empty() || subRoot.empty())
        throw INVALID_ARGUMENT("Name of root-directory is the empty string");
}

} // `RunPar` namespace
} // `bicast namespace
