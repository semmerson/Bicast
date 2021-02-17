/**
 * Program to subscribe to data-products via Hycast.
 *
 *        File: subscriber.cpp
 *  Created on: Aug 13, 2020
 *      Author: Steven R. Emmerson
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

#include "config.h"

#include "error.h"
#include "hycast.h"
#include "Node.h"
#include "P2pMgr.h"
#include "SockAddr.h"

#include <cstring>
#include <cstdio>
#include <exception>
#include <inttypes.h>
#include <iostream>
#include <limits.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

using namespace hycast;

using String = std::string;

// Runtime variables:
static InetAddr   mcastAddr;    ///< Multicast group IP address
static in_port_t  mcastPort;    ///< Multicast group port number
static InetAddr   pubAddr;      ///< Publisher's IP address
static InetAddr   p2pSrvrAddr;  ///< Local P2P server's IP address
static in_port_t  p2pSrvrPort;  ///< Local P2P server's port number
static int        listenSize;   ///< Local P2P server's `::listen()` size
static int        maxPeers;     ///< Maximum number of peers
static ServerPool rmtP2pSrvrs;  ///< Pool of remote P2P-servers
static String     repoRoot;     ///< Pathname of root of repository
static SegSize    segSize;      ///< Canonical data-segment size in bytes
static size_t     maxOpenFiles; ///< Maximum number of open files in repository

// Runtime variable defaults:
static InetAddr   defMcastAddr;    ///< Multicast group IP address
static String         mcastIpAddrDef = "232.128.117.1"; // Source-specific mcast
static in_port_t  defMcastPort;    ///< Multicast group port number
static InetAddr   defPubAddr;      ///< Publisher's IP address
static InetAddr   defP2pSrvrAddr;  ///< Local P2P server's IP address
static in_port_t  defP2pSrvrPort;  ///< Local P2P server's port number
static int        defListenSize;   ///< Local P2P server's `::listen()` size
static int        defMaxPeers;     ///< Maximum number of peers
static ServerPool defRmtP2pSrvrs;  ///< Pool of remote P2P-servers

/// Data-product publisher
static Publisher publisher{};

/**
 * Halts the publisher.
 *
 * @param[in] sig  Signal number. Ignored.
 */
static void sigHand(const int sig)
{
    publisher.halt(); // Gracefully terminate
}

/**
 * Sets signal handlers.
 */
static void setSigHand()
{
    log_setLevelSignal(SIGUSR2);

    struct sigaction sigact;
    (void) sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = &sigHand;
    (void)sigaction(SIGINT, &sigact, NULL);
    (void)sigaction(SIGTERM, &sigact, NULL);
}

/**
 * Assigns default values to the runtime parameters.
 */
static void init() noexcept
{
    // Initialize runtime parameters from defaults
    srvrIpAddr   = srvrIpAddrDef;
    srvrPort     = srvrPortDef;
    maxPeers     = defMaxPeers;
    listenSize   = defListenSize;
    minPort      = minPortDef;
    numPort      = numPortDef;
    mcastIpAddr  = mcastIpAddrDef;
    mcastPort    = defMcastPort;
    repoRoot     = repoRootDef;
    maxOpenFiles = maxOpenFilesDef;
    segSize      = segSizeDef;
}

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << log_getName() << " [-h]\n"
"    " << log_getName() << " [-A <mcastAddr>] [-a <srvrAddr>] [-b <minPort>]\n"
"        [-c <numPort>] [-f <maxOpenFiles>] [-l <level>] [-m <maxPeers>]\n"
"        [-P <mcastPort>] [-p <srvrPort>] [-q <listenSize>] [-r <repoRoot>]\n"
"        [-s <segSize>] [-y configFile>]\n"
"where:\n"
"    -A <mcastAddr>    IP address of multicast group. Default is \"" << mcastIpAddrDef << "\".\n"
"    -a <srvrAddr>     IP address of publisher's server. Default is \"" << srvrIpAddrDef << "\".\n"
"    -b <minPort>      Beginning port number for transitory servers. Default is\n"
"                      " << minPortDef << ".\n"
"    -f <maxOpenFiles> Maximum number of open repository files. Default is " << maxOpenFilesDef << ".\n"
"    -h                Print this help message on standard error then exit.\n"
"    -l <level>        Logging level. <level> is one of \"FATAL\", \"ERROR\", \"WARN\",\n"
"                     \"NOTE\", \"INFO\", \"DEBUG\", or \"TRACE\". Comparison is\n"
"                      case-insensitive and takes effect immediately. Default is\n"
"                     \"" << logLevelDef.to_string() << "\".\n"
"    -m <maxPeers>     Maximum number of connected peers. Default is " << defMaxPeers << ".\n"
"    -n <numPort>      Number of port numbers for transitory servers. Default is\n"
"                      " << numPortDef << ".\n"
"    -P <mcastPort>    Port number of multicast group. Default is " << defMcastPort << ".\n"
"    -p <srvrPort>     Port number of publisher's server. Default is " << srvrPortDef << ".\n"
"    -q <listenSize>   Size of the listening queue of the publisher's server.\n"
"                      Default is " << defListenSize << ".\n"
"    -r <repoRoot>     Pathname of the root of the publisher's repository.\n"
"                      Default is \"" << repoRootDef << "\".\n"
"    -s <segSize>      Size of a canonical data-segment in bytes. Default is\n"
"                      " << segSizeDef << ".\n"
"    -y <configFile>   Pathname of YAML configuration-file. Overrides previous\n"
"                      options; overridden by subsequent ones.\n";
}

template<class T>
static void tryDecode(
        YAML::Node&   parent,
        const String& key,
        T&            value)
{
    if (!parent.IsMap())
        throw INVALID_ARGUMENT("Node \"" + parent.Tag() + "\" isn't a map");

    auto child = parent[key];

    if (child) {
        if (!child.IsScalar())
            throw INVALID_ARGUMENT("Node \"" + key + "\" isn't scalar");

        value = child.as<T>();
    }
}

static void yamlInit(const String& pathname)
{
    auto config = YAML::LoadFile(pathname);

    try {
        if (config["LogLevel"]) {
            log_setLevel(config["LogLevel"].as<String>());
        }
        if (config["Peer2Peer"]) {
            auto p2p = config["Peer2Peer"];

            tryDecode<decltype(srvrIpAddr)>(p2p, "IpAddr", srvrIpAddr);
            tryDecode<decltype(srvrPort)>(p2p, "Port", srvrPort);
            tryDecode<decltype(listenSize)>(p2p, "ListenSize", listenSize);
            tryDecode<decltype(maxPeers)>(p2p, "MaxPeers", maxPeers);

            if (p2p["PortPool"]) {
                auto pool = config["PortPool"];

                tryDecode<decltype(minPort)>(pool, "Min", minPort );
                tryDecode<decltype(numPort)>(pool, "Num", numPort );
            }
        }

        if (config["Multicast"]) {
            auto mcast = config["Multicast"];

            tryDecode<decltype(mcastIpAddr)>(mcast, "IpAddr", mcastIpAddr);
            tryDecode<decltype(mcastPort)>(mcast, "Port", mcastPort);
        }

        if (config["Repository"]) {
            auto repo = config["Repository"];

            tryDecode<decltype(repoRoot)>(repo, "Pathname", repoRoot);
            tryDecode<decltype(maxOpenFiles)>(repo, "MaxOpenFiles", maxOpenFiles);
        }

        tryDecode<decltype(segSize)>(config, "SegmentSize", segSize);
    } // YAML file loaded
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse YAML file \"" +
                pathname + "\""));
    }
}

/**
 * Vets the runtime parameters.
 *
 * @throw std::invalid_argument  A runtime parameter is invalid
 */
static void vetRunPars()
{
    InetAddr(srvrIpAddr);
    if (maxPeers <= 0)
        throw INVALID_ARGUMENT("Maximum number of peers is not positive");
    if (minPort == 0)
        throw INVALID_ARGUMENT("Minimum port number for temporary servers is zero");
    if (numPort == 0)
        throw INVALID_ARGUMENT("Number of ports for temporary servers is zero");
    InetAddr(mcastIpAddr);
    if (mcastPort == 0)
        throw INVALID_ARGUMENT("Multicast port number is zero");
    if (repoRoot.empty())
        throw INVALID_ARGUMENT("Pathname of root of repository is empty");
    if (maxOpenFiles == 0)
        throw INVALID_ARGUMENT("Maximum number of open repository files is zero");
    if (maxOpenFiles > sysconf(_SC_OPEN_MAX))
        throw INVALID_ARGUMENT("Maximum number of open repository files is "
                "greater than " + std::to_string(sysconf(_SC_OPEN_MAX)));
    if (segSize == 0)
        throw INVALID_ARGUMENT("Canonical data-segment size is zero");
}

/**
 * Sets the runtime parameters.
 *
 * @throw std::invalid_argument  Invalid option, option argument, or variable value
 */
static void getRunPars(
        const int    argc, ///< Number of command-line arguments
        char* const* argv) ///< Command-line arguments
{
    log_setName(::basename(argv[0]));
    init(); // Ensure default values initially

    opterr = 0;    // 0 => getopt() won't write to `stderr`
    int c;
    while ((c = ::getopt(argc, argv, ":A:a:b:f:hl:m:n:P:p:q:r:s:y:")) != -1) {
        switch (c) {
        case 'A': {
            mcastIpAddr = optarg;
            break;
        }
        case 'a': {
            srvrIpAddr = optarg;
            break;
        }
        case 'b': {
            if (sscanf(optarg, "%hu", &minPort) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'c': {
            if (sscanf(optarg, "%hu", &numPort) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'f': {
            if (sscanf(optarg, "%zu", &maxOpenFiles) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'h': {
            usage();
            exit(0);
        }
        case 'l': {
            log_setLevel(optarg);
            break;
        }
        case 'm': {
            if (sscanf(optarg, "%d", &maxPeers) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'P': {
            if (sscanf(optarg, "%hu", &mcastPort) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'p': {
            if (sscanf(optarg, "%hu", &srvrPort) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'q': {
            if (sscanf(optarg, "%d", &listenSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'r': {
            repoRoot = String(optarg);
            break;
        }
        case 's': {
            if (sscanf(optarg, "%hu", &segSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'y': {
            try {
                yamlInit(optarg);
            }
            catch (const std::exception& ex) {
                std::throw_with_nested(INVALID_ARGUMENT(
                        String("Couldn't initialize using configuration-file \"")
                        + optarg + "\""));
            }
            break;
        }
        case ':': { // Missing option argument. Due to leading ":" in opt-string
            throw INVALID_ARGUMENT(String("Invalid \"-") +
                static_cast<char>(optopt) + "\" option");
        }
        default : { // c == '?'
            throw INVALID_ARGUMENT(String("Unknown \"-") +
                    static_cast<char>(optopt) + "\" option");
        }
        } // `switch` statement
    } // While getopt() loop

    if (optind != argc)
        throw INVALID_ARGUMENT("Excess arguments");

    vetRunPars();
}

/**
 * Publishes data-products.
 *
 * @param[in] argc  Number of command-line arguments
 * @param[in] argv  Command-line arguments
 * @retval    0     Success
 * @retval    1     Command-line error
 * @retval    2     Runtime error
 */
int main(
        const int    argc, ///< Number of command-line arguments
        char* const* argv) ///< Command-line arguments
{
    std::set_terminate(&terminate); // NB: Hycast version

    try {
        getRunPars(argc, argv);

        auto    repo = PubRepo(repoRoot, segSize, maxOpenFiles);
        auto    mcastGrpAddr = SockAddr(mcastIpAddr, mcastPort);
        P2pInfo p2pInfo;

        p2pInfo.sockAddr = SockAddr(srvrIpAddr, srvrPort);
        p2pInfo.listenSize = listenSize;
        p2pInfo.maxPeers = maxPeers;
        p2pInfo.portPool = PortPool(minPort, numPort);

        publisher = Publisher(p2pInfo, mcastGrpAddr, repo);

        setSigHand(); // Catches termination signals
        publisher();
    }
    catch (const std::invalid_argument& ex) {
        LOG_FATAL(ex);
        usage();
        return 1;
    }
    catch (const std::exception& ex) {
        LOG_FATAL(ex);
        return 2;
    }

    return 0;
}
