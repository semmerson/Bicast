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

String      feedName;          ///< Name of the feed
SysDuration heartbeatInterval; ///< Time interval between heartbeat packets. <0 => no heartbeat
LogLevel    logLevel;          ///< Initial logging level
int         maxNumPeers;       ///< Maximum number of neighboring peers
int         maxOpenProds;      ///< Maximum number of product-files to keep open
SegSize     maxSegSize;        ///< Size of a canonical data-segment
SockAddr    mcastDstAddr;      ///< Multicast group destination address
InetAddr    mcastSrcAddr;      ///< Multicast group source address
SockAddr    p2pSrvrAddr;       ///< Address of the P2P server
int         p2pSrvrQSize;      ///< Maximum number of pending P2P connections
SysDuration peerEvalInterval;  ///< Time interval for evaluating peer performance
SysDuration prodKeepTime;      ///< Amount of time to keep products before deleting them
String      progName;          ///< Program name
String      pubRoot;           ///< Pathname of the root of the publisher's directory
SockAddr    pubSrvrAddr;       ///< Address of the publisher's server (not the P2P server)
int         pubSrvrQSize;      ///< Maximum number of pending connections to publisher's server
int         trackerCap;        ///< Maximum number of potential P2P servers to track & exchange

static void setDefaults()
{
    feedName = String("Bicast");
    heartbeatInterval = SysDuration(std::chrono::seconds(30));
    logLevel = LogLevel::NOTE;
    maxNumPeers = 8;
    maxOpenProds = _SC_OPEN_MAX/2;
    maxSegSize = 20000;
    mcastDstAddr = SockAddr("232.1.1.1:38800");
    mcastSrcAddr = InetAddr();
    p2pSrvrAddr = SockAddr();
    p2pSrvrQSize = 8;
    peerEvalInterval = SysDuration(std::chrono::minutes(5));
    prodKeepTime = SysDuration(std::chrono::hours(1));
    progName = String("<unset>");
    pubRoot = String("./pubRoot");
    pubSrvrAddr = SockAddr("0.0.0.0:38800");
    pubSrvrQSize = 256;
    trackerCap = 1000;
}

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << progName << " -h\n"
"    " << progName << " [options]\n"
"Options:\n"
"  General:\n"
"    -c <configFile>   Pathname of configuration-file. Overrides previous\n"
"                      arguments; overridden by subsequent ones.\n"
"    -d <maxSegSize>   Maximum data-segment size in bytes. Default is " <<
                       maxSegSize << ".\n"
"    -f <name>         Name of data-product feed. Default is \"" << feedName << "\".\n"
"    -h                Print this help message on standard error, then exit.\n"
"    -l <logLevel>     Logging level. <level> is \"FATAL\", \"ERROR\", \"WARN\",\n"
"                      \"NOTE\", \"INFO\", \"DEBUG\", or \"TRACE\". Comparison is case-\n"
"                      insensitive and takes effect immediately. Default is\n" <<
"                      \"" << logLevel << "\".\n"
"    -r <pubRoot>      Pathname of publisher's root-directory. Default is \"" <<
                       pubRoot << "\".\n"
"    -t <trackerCap>   Maximum number of P2P servers to track. Default is " <<
                       trackerCap << ".\n"
"  Publisher's Server:\n"
"    -P <pubAddr>      Socket address of publisher's server (not the P2P server).\n"
"                      Default is \"" << pubSrvrAddr << "\".\n"
"    -Q <maxPending>   Maximum number of pending connections to publisher's\n"
"                      server (not the P2P server). Default is " << pubSrvrQSize << ".\n"
"  Multicasting:\n"
"    -m <dstAddr>      Destination address of multicast group. Default is\n" <<
"                      \"" << mcastDstAddr << "\".\n"
"    -s <srcAddr>      Internet address of multicast source/interface. Must not\n"
"                      be wildcard. Default is determined by operating system\n"
"                      based on destination address of multicast group.\n"
"  Peer-to-Peer:\n"
"    -b <interval>     Time between heartbeat packets in seconds. <0 => no\n"
"                      heartbeat. Default is " << heartbeatInterval.count()*sysClockRatio << ".\n"
"    -e <evalTime>     Peer evaluation duration, in seconds, before replacing\n"
"                      poorest performer. Default is " << peerEvalInterval.count()*sysClockRatio <<
                       ".\n"
"    -n <maxPeers>     Maximum number of connected peers. Default is " <<
                       maxNumPeers << ".\n"
"    -p <p2pAddr>      Socket address for local P2P server (not the publisher's\n"
"                      server). IP address must not be wildcard. Default IP\n"
"                      address is that of interface used for multicasting.\n"
"                      Default port number is 0.\n"
"    -q <maxPending>   Maximum number of pending connections to P2P server (not\n"
"                      the publisher's server). Default is " << p2pSrvrQSize <<
                       ".\n"
"  Repository:\n"
"    -k <keepTime>     How long to keep data-products in seconds. Default is\n" <<
"                      " << prodKeepTime.count()*sysClockRatio << ".\n"
"    -o <maxOpenFiles> Maximum number of open repository files. Default is " << maxOpenProds <<
                       ".\n"
"\n"
"SIGUSR2 rotates the logging level.\n";
}

/**
 * Sets runtime parameters from a configuration-file.
 *
 * @param[in] pathname           Pathname of the configuration-file
 * @throw std::runtime_error     Parser failure
 */
static void setFromConfig(const String& pathname)
{
    auto node0 = YAML::LoadFile(pathname);

    try {
        Parser::tryDecode<decltype(pubRoot)>(node0, "pubRoot", pubRoot);
        Parser::tryDecode<decltype(feedName)>(node0, "name", feedName);
        auto node1 = node0["logLevel"];
        if (node1)
            log_setLevel(node1.as<String>());
        Parser::tryDecode<decltype(maxSegSize)>(node0, "maxSegSize", maxSegSize);
        Parser::tryDecode<decltype(trackerCap)>(node0, "trackerCap", trackerCap);

        node1 = node0["server"];
        if (node1) {
            auto node2 = node1["pubAddr"];
            if (node2)
                pubSrvrAddr = SockAddr(node2.as<String>());
            Parser::tryDecode<decltype(pubSrvrQSize)>(node1, "maxPending", pubSrvrQSize);
        }

        node1 = node0["multicast"];
        if (node1) {
            auto node2 = node1["dstAddr"];
            if (node2)
                mcastDstAddr = SockAddr(node2.as<String>());
            node2 = node1["srcAddr"];
            if (node2)
                mcastSrcAddr = InetAddr(node2.as<String>());
        }

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
            int seconds;
            Parser::tryDecode<decltype(seconds)>(node1, "keepTime", seconds);
            prodKeepTime = SysDuration(std::chrono::seconds(seconds));
        }
    } // YAML file loaded
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse YAML file \"" + pathname + "\""));
    }
}

/// Vets the runtime parameters parameters.
static void vetRunPar()
{
    if (mcastDstAddr.getInetAddr().getFamily() != mcastSrcAddr.getFamily())
        throw INVALID_ARGUMENT("Address of multicast group (" + mcastDstAddr.to_string() +
                ") and address of multicast source (" + mcastSrcAddr.to_string() +
                ") belong to different address families");

    if (peerEvalInterval.count() <= 0)
        throw INVALID_ARGUMENT("Peer performance evaluation-interval is not positive");

    if (heartbeatInterval == SysDuration(0))
        throw INVALID_ARGUMENT("Heartbeat interval cannot be zero");

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
        throw INVALID_ARGUMENT("How long to keep repository files is not positive");

    if (pubSrvrQSize <= 0)
        throw INVALID_ARGUMENT("Size of publisher's server-queue is not positive");

    if (p2pSrvrQSize <= 0)
        throw INVALID_ARGUMENT("Size of P2P server-queue is not positive");

    if (pubRoot.empty())
        throw INVALID_ARGUMENT("Name of publisher's root-directory is the empty string");

    if (maxSegSize <= 0)
        throw INVALID_ARGUMENT("Maximum size of a data-segment is not positive");
    if (maxSegSize > UdpSock::MAX_PAYLOAD)
        throw INVALID_ARGUMENT("Maximum size of a data-segment (" +
                std::to_string(maxSegSize) + ") is greater than UDP maximum (" +
                std::to_string(UdpSock::MAX_PAYLOAD) + ")");
}

void init(
        int          argc,
        char* const* argv)
{
    setDefaults();

    progName = String(::basename(argv[0]));

    try {
        opterr = 0;    // 0 => getopt() won't write to `stderr`
        int c;
        while ((c = ::getopt(argc, argv, ":c:d:e:f:hk:l:m:o:P:p:Q:q:r:s:t:")) != -1) {
            switch (c) {
            case 'b': {
                int interval;
                if (::sscanf(optarg, "%d", &interval) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                heartbeatInterval = std::chrono::seconds(interval);
                break;
            }
            case 'h': {
                usage();
                exit(0);
            }

            case 'c': {
                try {
                    setFromConfig(optarg);
                }
                catch (const std::exception& ex) {
                    std::throw_with_nested(INVALID_ARGUMENT(
                            String("Couldn't initialize using configuration-file \"") + optarg +
                            "\""));
                }
                break;
            }
            case 'd': {
                int size;
                if (::sscanf(optarg, "%d", &size) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                maxSegSize = size;
                break;
            }
            case 'e': {
                int evalTime;
                if (::sscanf(optarg, "%d", &evalTime) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                peerEvalInterval = SysDuration(std::chrono::seconds(evalTime));
                break;
            }
            case 'f': {
                feedName = String(optarg);
                break;
            }
            case 'k': {
                int keepTime;
                if (::sscanf(optarg, "%d", &keepTime) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                prodKeepTime = SysDuration(std::chrono::seconds(keepTime));
                break;
            }
            case 'l': {
                log_setLevel(optarg);
                logLevel = log_getLevel();
                break;
            }
            case 'm': {
                mcastDstAddr = SockAddr(optarg);
                break;
            }
            case 'n': {
                int maxPeers;
                if (::sscanf(optarg, "%d", &maxPeers) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                maxNumPeers = maxPeers;
                break;
            }
            case 'o': {
                if (::sscanf(optarg, "%d", &maxOpenProds) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                break;
            }
            case 'P': {
                pubSrvrAddr = SockAddr(optarg);
                break;
            }
            case 'p': {
                p2pSrvrAddr = SockAddr(optarg);
                break;
            }
            case 'Q': {
                if (::sscanf(optarg, "%d", &pubSrvrQSize) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                break;
            }
            case 'q': {
                if (::sscanf(optarg, "%d", &p2pSrvrQSize) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                break;
            }
            case 'r': {
                pubRoot = String(optarg);
                break;
            }
            case 's': {
                mcastSrcAddr = InetAddr(optarg);
                break;
            }
            case 't': {
                if (::sscanf(optarg, "%d", &trackerCap) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                break;
            }
            case ':': { // Missing option argument. Due to leading ":" in opt-string
                throw INVALID_ARGUMENT(String("Option \"-") + static_cast<char>(optopt) +
                        "\" is missing an argument");
            }
            default : { // c == '?'
                throw INVALID_ARGUMENT(String("Unknown \"-") + static_cast<char>(optopt) +
                        "\" option");
            }
            } // `switch` statement
        } // While getopt() loop

        if (optind != argc)
            throw LOGIC_ERROR("Too many operands specified");

        if (!mcastSrcAddr)
            mcastSrcAddr = UdpSock(mcastDstAddr).getLclAddr().getInetAddr();

        if (!p2pSrvrAddr)
            p2pSrvrAddr = SockAddr(mcastSrcAddr);

        vetRunPar();
    }
    catch (const std::exception& ex) {
        usage();
        ::exit(1);
    }
}

}
}
