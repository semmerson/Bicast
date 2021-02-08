/**
 * Program to publish data-products via Hycast.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: publisher.cpp
 *  Created on: Aug 13, 2020
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "hycast.h"
#include "Node.h"
#include "P2pMgr.h"
#include "PortPool.h"
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

// Runtime parameters:
static InetAddr  p2pInetAddr;   ///< Local P2P server's Internet address
static in_port_t p2pPort;       ///< Local P2P server's port number
static InetAddr  mcastInetAddr; ///< Multicast group's Internet address
static in_port_t mcastPort;     ///< Multicast group's port number
static unsigned  maxPeers;      ///< Maximum number of connected peers
static unsigned  listenSize;    ///< Local P2P server's `::listen()` size
static String    repoRoot;      ///< Pathname of root of publisher's repository
static size_t    maxOpenFiles;  ///< Maximum number of open repository files
static SegSize   segSize;       ///< Size of canonical data-segment in bytes.

// Runtime parameter defaults:
static const InetAddr  defP2pInetAddr  = InetAddr("0.0.0.0");
static const in_port_t defP2pPort      = 38800;
static const in_port_t defMcastPort    = defP2pPort;
static const LogLevel  defLogLevel     = log_getLevel();
static unsigned        defMaxPeers     = 8;
static unsigned        defListenSize   = defMaxPeers;
static const String    defRepoRoot("repo");    // In current working directory
static const size_t    defMaxOpenFiles = _POSIX_OPEN_MAX/2;
static const SegSize   defSegSize      = 1444; // Maximum ethernet UDP payload

/// Data-product publisher
static Publisher publisher{};

/**
 * Halts the publisher.
 *
 * @param[in] sig  Signal number. Ignored.
 */
static void sigHandler(const int sig)
{
    publisher.halt(); // Gracefully terminate
}

/**
 * Sets signal handling.
 */
static void setSigHandling()
{
    log_setLevelSignal(SIGUSR2);

    struct sigaction sigact;
    (void) sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0; // Don't restart system calls
    sigact.sa_handler = &sigHandler;
    (void)sigaction(SIGINT, &sigact, NULL);
    (void)sigaction(SIGTERM, &sigact, NULL);
}

/**
 * Assigns default values to appropriate runtime parameters.
 */
static void init() noexcept
{
    // Initialize runtime parameters that have defaults
    p2pInetAddr  = defP2pInetAddr;
    p2pPort      = defP2pPort;
    mcastPort    = defMcastPort;
    maxPeers     = defMaxPeers;
    listenSize   = defListenSize;
    repoRoot     = defRepoRoot;
    maxOpenFiles = defMaxOpenFiles;
    segSize      = defSegSize;
}

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << log_getName() << " [-h]\n"
"    " << log_getName() << "[-i <p2pInetAddr>] [-l <level>] [-m <maxPeers>]\n"
"        [-o <maxOpenFiles>] [-P <mcastPort>] [-p <p2pPort>] "
             "[-q <listenSize>]\n"
"        [-r <repoRoot>] [-s <segSize>] [-y configFile>] [<mcastInetAddr>]\n"
"where:\n"
"    -h                Print this help message on standard error, then exit.\n"
"    -i <p2pInetAddr>  Internet address of local P2P server. Default is\n"
"                      \"" << defP2pInetAddr.to_string() << "\".\n"
"    -l <level>        Logging level. <level> is one of \"FATAL\", \"ERROR\", "
                           "\"WARN\",\n"
"                      \"NOTE\", \"INFO\", \"DEBUG\", or \"TRACE\". Comparison "
                           "is case-\n"
"                      insensitive and takes effect immediately. Default is\n"
"                      \"" << defLogLevel.to_string() << "\".\n"
"    -m <maxPeers>     Maximum number of connected peers. Default is " <<
                           defMaxPeers << ".\n"
"    -o <maxOpenFiles> Maximum number of open repository files. Default is " <<
                           defMaxOpenFiles << ".\n"
"    -P <mcastPort>    Port number of multicast group. Default is " <<
                           defMcastPort << ".\n"
"    -p <p2pPort>      Port number of local P2P server. Default is " <<
                           defP2pPort << ".\n"
"    -q <listenSize>   Size of listening queue of publisher's P2P server. "
                           "Default\n"
"                      is " << defListenSize << ".\n"
"    -r <repoRoot>     Pathname of root of publisher's repository. Default is\n"
"                      \"" << defRepoRoot << "\".\n"
"    -s <segSize>      Size of a canonical data-segment in bytes. Default is\n"
"                      " << defSegSize << ".\n"
"    -y <configFile>   Pathname of YAML configuration-file. Overrides "
                           "previous\n"
"                      arguments; overridden by subsequent ones.\n"
"\n"
"    <mcastInetAddr>   Internet address of multicast group. Optional if "
                           "specified\n"
"                      in configuration-file.\n";
}

/**
 * Tries to decode a scalar (i.e., primitive) value in a YAML map.
 *
 * @tparam     T                 Type of scalar value
 * @param[in]  parent            Map containing the scalar
 * @param[in]  key               Name of the scalar
 * @param[out] value             Scalar value
 * @return     `true`            Success. `value` is set.
 * @return     `false`           No scalar with given name
 * @throw std::invalid_argument  Parent node isn't a map
 * @throw std::invalid_argument  Subnode with given name isn't a scalar
 */
template<class T>
static bool tryDecode(YAML::Node&   parent,
                      const String& key,
                      T&            value)
{
    if (!parent.IsMap())
        throw INVALID_ARGUMENT("Node \"" + parent.Tag() + "\" isn't a map");

    auto child = parent[key];

    if (!child)
        return false;

    if (!child.IsScalar())
        throw INVALID_ARGUMENT("Node \"" + key + "\" isn't scalar");

    value = child.as<T>();
    return true;
}

/**
 * Sets runtime parameters from a YAML file.
 *
 * @param[in] pathname           Pathname of the YAML file
 * @throw std::runtime_error     YAML parser failure
 */
static void yamlInit(const String& pathname)
{
    auto rootNode = YAML::LoadFile(pathname);

    try {
        auto node = rootNode["LogLevel"];
        if (node)
            log_setLevel(node.as<String>());

        node = rootNode["P2pServer"];
        if (node) {
            String inetAddr;

            if (tryDecode<String>(node, "InetAddr", inetAddr))
                p2pInetAddr = InetAddr(inetAddr);
            tryDecode<decltype(p2pPort)>(node, "Port", p2pPort);
            tryDecode<decltype(listenSize)>(node, "ListenSize", listenSize);
            tryDecode<decltype(maxPeers)>(node, "MaxPeers", maxPeers);
        }

        node = rootNode["Multicast"];
        if (node) {
            String inetAddr;

            if (tryDecode<String>(node, "InetAddr", inetAddr))
                mcastInetAddr = InetAddr(inetAddr);
            tryDecode<decltype(mcastPort)>(node, "Port", mcastPort);
        }

        node = rootNode["Repository"];
        if (node) {
            tryDecode<decltype(repoRoot)>(node, "Pathname", repoRoot);
            tryDecode<decltype(maxOpenFiles)>(node, "MaxOpenFiles",
                    maxOpenFiles);
        }

        tryDecode<decltype(segSize)>(rootNode, "SegmentSize", segSize);
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
 * @throw std::logic_error       A necessary runtime parameter wasn't specified
 */
static void vetRunPars()
{
    if (!p2pInetAddr)
        throw LOGIC_ERROR("Internet address of publisher's P2P server wasn't specified");
    if (p2pPort == 0)
        throw INVALID_ARGUMENT("P2P server's port number is zero");
    if (maxPeers <= 0)
        throw INVALID_ARGUMENT("Maximum number of peers is not positive");
    if (listenSize <= 0)
        throw INVALID_ARGUMENT("P2P server's listen() size is not positive");

    if (mcastPort == 0)
        throw INVALID_ARGUMENT("Multicast group's port number is zero");
    if (!mcastInetAddr)
        throw LOGIC_ERROR("Internet address of multicast group wasn't specified");
    if (!mcastInetAddr.isSsm())
        throw INVALID_ARGUMENT("Multicast group's Internet address isn't "
                "source-specific");

    if (repoRoot.empty())
        throw INVALID_ARGUMENT("Pathname of publisher's repository wasn't specified");
    if (maxOpenFiles == 0)
        throw INVALID_ARGUMENT("Maximum number of open repository files is zero");
    if (maxOpenFiles > sysconf(_SC_OPEN_MAX))
        throw INVALID_ARGUMENT("Maximum number of open repository files is "
                "greater than system maximum, " +
                std::to_string(sysconf(_SC_OPEN_MAX)));

    if (segSize <= 0)
        throw INVALID_ARGUMENT("Canonical data-segment size is not positive");
}

/**
 * Sets the runtime parameters.
 *
 * @param[in] argc               Number of command-line arguments
 * @param[in] argv               Command-line arguments
 * @throw std::invalid_argument  Invalid option, option argument, or operand
 * @throw std::logic_error       Too many or too few operands
 */
static void getRunPars(
        const int    argc,
        char* const* argv)
{
    log_setName(::basename(argv[0]));
    init(); // Ensure default values initially

    opterr = 0;    // 0 => getopt() won't write to `stderr`
    int c;
    while ((c = ::getopt(argc, argv, ":hi:l:m:o:P:p:q:r:s:y:")) != -1) {
        switch (c) {
        case 'h': {
            usage();
            exit(0);
        }
        case 'i': {
            p2pInetAddr = InetAddr(optarg);
        }
        case 'l': {
            log_setLevel(optarg);
            break;
        }
        case 'm': {
            if (::sscanf(optarg, "%u", &maxPeers) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'o': {
            if (::sscanf(optarg, "%zu", &maxOpenFiles) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'P': {
            if (::sscanf(optarg, "%hu", &mcastPort) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'p': {
            if (::sscanf(optarg, "%hu", &p2pPort) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'q': {
            if (::sscanf(optarg, "%d", &listenSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") +
                    static_cast<char>(c) + "\" option");
            break;
        }
        case 'r': {
            repoRoot = String(optarg);
            break;
        }
        case 's': {
            if (::sscanf(optarg, "%hu", &segSize) != 1)
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

    if (!mcastInetAddr && optind >= argc)
        throw LOGIC_ERROR("Internet address of multicast group wasn't specified");
    mcastInetAddr = InetAddr(argv[optind]);
    ++optind;

    if (optind != argc)
        throw LOGIC_ERROR("Too many operands specified");

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
int main(const int    argc,
         char* const* argv)
{
    std::set_terminate(&terminate); // NB: Hycast version

    try {
        getRunPars(argc, argv);

        auto    repo = PubRepo(repoRoot, segSize, maxOpenFiles);
        P2pInfo p2pInfo;

        p2pInfo.sockAddr = SockAddr(p2pInetAddr, p2pPort);
        p2pInfo.listenSize = listenSize;
        p2pInfo.maxPeers = maxPeers;

        const auto mcastSockAddr = SockAddr(mcastInetAddr, mcastPort);
        publisher = Publisher(p2pInfo, mcastSockAddr, repo);

        setSigHandling(); // Catches termination signals
        publisher();
    }
    catch (const std::invalid_argument& ex) {
        LOG_FATAL(ex);
        usage();
        return 1;
    }
    catch (const std::logic_error& ex) {
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
