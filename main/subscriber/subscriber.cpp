/**
 * Program to subscribe to data-products via Hycast.
 *
 *        File: subscriber.cpp
 *  Created on: Aug 13, 2020
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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

#include "Node.h"
#include "SubInfo.h"
#include "ThreadException.h"

#include <semaphore.h>
#include <yaml-cpp/yaml.h>

using namespace hycast;

using String = std::string;

/// Runtime parameters of this program
struct RunPar {
    LogLevel  logLevel;   ///< Logging level
    SockAddr  pubAddr;    ///< Address of publisher
    InetAddr  mcastIface; ///< Address of interface to use to receive multicast. May be wildcard.
    struct P2pArgs {
        struct SrvrArgs {
            SockAddr addr;       ///< P2P server's address. Must not be wildcard.
            int      listenSize; ///< Size of `::listen()` queue. Don't use 0.
            SrvrArgs(
                    const SockAddr& addr,
                    const int       listenSize)
                : addr(addr)
                , listenSize(listenSize)
            {}
        }         srvr;
        int      trackerSize; ///< Size of tracker object
        int      maxPeers;    ///< Maximum number of peers to have
        int      evalTime;    ///< Time interval for evaluating peer performance
        P2pArgs(const SockAddr& addr,
                const int       listenSize,
                const int       trackerSize,
                const int       maxPeers,
                const int       evalTime)
            : srvr(addr, listenSize)
            , trackerSize(trackerSize)
            , maxPeers(maxPeers)
            , evalTime(evalTime)
        {}
    }         p2p;
    struct RepoArgs {
        String   rootDir;      ///< Pathname of the repository's root directory
        int      maxOpenFiles; ///< Maximum number of open files
        RepoArgs(
                const String&  rootDir,
                const unsigned maxOpenFiles)
            : rootDir(rootDir)
            , maxOpenFiles(maxOpenFiles)
        {}
    }         repo;

    RunPar()
        : logLevel(LogLevel::NOTE)
        , pubAddr()
        , mcastIface("0.0.0.0")
        , p2p(SockAddr(), 8, 100, 8, 60)
        , repo("repo", ::sysconf(_SC_OPEN_MAX)/2)
    {}
};

static sem_t             sem;       ///< Semaphore for async-signal-safe state changes
static std::atomic<bool> stop;      ///< Should the program stop?
static ThreadEx          threadEx;  ///< Exception thrown by a thread
static RunPar            runPar;    ///< Runtime parameters:
const static RunPar      defRunPar; ///< Default runtime parameters
static SubNode::Pimpl    subNode;   ///< Data-product subscribing node
static SubInfo           subInfo;   ///< Subscription information from publisher
static Tracker           tracker;   ///< Addresses of P2P servers

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << log_getName() << " -h\n"
"    " << log_getName() << " [-c <configFile>] [-e <evalTime>] [-l <level>] [-m <maxPeers>]\n"
"        [-o <maxOpenFiles>] [-q <listenSize>] [-r <repoRoot>] [-t <trackerSize>]\n"
"        <pubAddr> <p2pAddr>\n"
"where:\n"
"    -h                Print this help message on standard error, then exit.\n"
"\n"
"    -c <configFile>   Pathname of configuration-file. Overrides previous\n"
"                      arguments; overridden by subsequent ones.\n"
"    -e <evalTime>     Peer evaluation duration, in seconds, before replacing\n"
"                      poorest performer. Default is " << defRunPar.p2p.evalTime << ".\n"
"    -l <level>        Logging level. <level> is \"FATAL\", \"ERROR\", \"WARN\",\n"
"                      \"NOTE\", \"INFO\", \"DEBUG\", or \"TRACE\". Comparison is case-\n"
"                      insensitive and takes effect immediately. Default is\n" <<
"                      \"" << defRunPar.logLevel << "\".\n"
"    -m <maxPeers>     Maximum number of connected peers. Default is " << defRunPar.p2p.maxPeers <<
                       ".\n"
"    -o <maxOpenFiles> Maximum number of open repository files. Default is " <<
                       defRunPar.repo.maxOpenFiles << ".\n"
"    -q <listenSize>   Size of P2P server's listen() queue. Default is " <<
                       defRunPar.p2p.srvr.listenSize << ".\n"
"    -r <repoRoot>     Pathname of root of publisher's repository. Default is\n"
"                      \"" << defRunPar.repo.rootDir << "\".\n"
"    -t <trackerSize>  Maximum size of the list of remote P2P servers. Default is\n" <<
"                      " << defRunPar.p2p.trackerSize << ".\n"
"\n"
"    <pubAddr>         Socket address of the publisher\n"
"    <p2pAddr>         Internet address for local P2P server\n"
"\n"
"SIGUSR2 rotates the logging level.\n";
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
        auto node1 = node0["LogLevel"];
        if (node1)
            log_setLevel(node1.as<String>());

        node1 = node0["Publisher"];
        if (node1)
            runPar.pubAddr = SockAddr(node1.as<String>(), 0);

        node1 = node0["Peer2Peer"];
        if (node1) {
            auto node2 = node1["Server"];
            if (node2) {
                auto node2 = node1["InetAddr"];
                if (node2)
                    runPar.p2p.srvr.addr = SockAddr(node2.as<String>(), 0);

                tryDecode<decltype(runPar.p2p.srvr.listenSize)>(node2, "ListenSize",
                        runPar.p2p.srvr.listenSize);
            }

            tryDecode<decltype(runPar.p2p.maxPeers)>(node1, "MaxPeers", runPar.p2p.maxPeers);
            tryDecode<decltype(runPar.p2p.trackerSize)>(node1, "TrackerSize",
                    runPar.p2p.trackerSize);

            node2 = node1["ReplaceTrigger"];
            if (node2) {
                auto node3 = node1["Type"];
                if (node3 && node3.as<String>() == "time")
                    tryDecode<decltype(runPar.p2p.evalTime)>(node2, "Duration",
                            runPar.p2p.evalTime);
            }
        }

        node1 = node0["Repository"];
        if (node1) {
            tryDecode<decltype(runPar.repo.rootDir)>(node1, "Pathname", runPar.repo.rootDir);
            tryDecode<decltype(runPar.repo.maxOpenFiles)>(node1, "MaxOpenFiles",
                    runPar.repo.maxOpenFiles);
        }
    } // YAML file loaded
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse YAML file \"" + pathname + "\""));
    }
}

/**
 * Vets the runtime parameters.
 *
 * @throw std::invalid_argument  A runtime parameter is invalid
 */
static void vetRunPars()
{
    if (!runPar.pubAddr)
        throw INVALID_ARGUMENT("Publisher's socket address wasn't specified");

    if (!runPar.p2p.srvr.addr)
        throw INVALID_ARGUMENT("IP address for local P2P server wasn't specified");
    if (runPar.p2p.srvr.listenSize <= 0)
        throw INVALID_ARGUMENT("Size of local P2P server's listen() queue is not positive");
    if (runPar.p2p.maxPeers <= 0)
        throw INVALID_ARGUMENT("Maximum number of peers is not positive");
    if (runPar.p2p.trackerSize <= 0)
        throw INVALID_ARGUMENT("Tracker size is not positive");
    if (runPar.p2p.evalTime <= 0)
        throw INVALID_ARGUMENT("Peer performance evaluation-time is not positive");

    if (runPar.repo.rootDir.empty())
        throw INVALID_ARGUMENT("Name of repository's root-directory is the empty string");
    if (runPar.repo.maxOpenFiles <= 0)
        throw INVALID_ARGUMENT("Maximum number of open repository files is not positive");
    if (runPar.repo.maxOpenFiles > ::sysconf(_SC_OPEN_MAX))
        throw INVALID_ARGUMENT("Maximum number of open repository files is "
                "greater than system maximum, " + std::to_string(sysconf(_SC_OPEN_MAX)));
}

/// Initializes non-command-line runtime parameters
static void init()
{
    if (sem_init(&sem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");

    tracker = Tracker(runPar.p2p.trackerSize);
}

/**
 * Sets runtime parameters from the command-line.
 *
 * @throw std::invalid_argument  Invalid option, option argument, or variable value
 */
static void getCmdPars(
        const int    argc, ///< Number of command-line arguments
        char* const* argv) ///< Command-line arguments
{
    log_setName(::basename(argv[0]));
    runPar = defRunPar;

    opterr = 0;    // 0 => getopt() won't write to `stderr`
    int c;
    while ((c = ::getopt(argc, argv, ":c:e:l:m:o:q:r:t:")) != -1) {
        switch (c) {
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
                        String("Couldn't initialize using configuration-file \"") + optarg + "\""));
            }
            break;
        }
        case 'e': {
            int evalTime;
            if (::sscanf(optarg, "%d", &evalTime) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (evalTime <= 0)
                throw INVALID_ARGUMENT("Peer performance evaluation-time is not positive");
            runPar.p2p.evalTime = evalTime;
            break;
        }
        case 'l': {
            log_setLevel(optarg);
            runPar.logLevel = log_getLevel();
            break;
        }
        case 'm': {
            int maxPeers;
            if (::sscanf(optarg, "%d", &maxPeers) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (maxPeers <= 0)
                throw INVALID_ARGUMENT("Maximum number of peers is not positive");
            runPar.p2p.maxPeers = maxPeers;
            break;
        }
        case 'o': {
            if (::sscanf(optarg, "%ld", &runPar.repo.maxOpenFiles) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (runPar.repo.maxOpenFiles <= 0)
                throw INVALID_ARGUMENT("Maximum number of open repository files is not positive");
            if (runPar.repo.maxOpenFiles > sysconf(_SC_OPEN_MAX))
                throw INVALID_ARGUMENT("Maximum number of open repository files is "
                        "greater than system maximum, " + std::to_string(sysconf(_SC_OPEN_MAX)));
            break;
        }
        case 'q': {
            if (::sscanf(optarg, "%d", &runPar.p2p.srvr.listenSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (runPar.p2p.srvr.listenSize <= 0)
                throw INVALID_ARGUMENT("Size of P2P server's listen() queue is not positive");
            break;
        }
        case 'r': {
            runPar.repo.rootDir = String(optarg);
            if (runPar.repo.rootDir.empty())
                throw INVALID_ARGUMENT("Name of repository's root-directory is the empty string");
            break;
        }
        case 't': {
            int trackerSize;
            if (::sscanf(optarg, "%d", &trackerSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (trackerSize <= 0)
                throw INVALID_ARGUMENT("Tracker size is not positive");
            runPar.p2p.trackerSize = trackerSize;
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

    if (argv[optind] == nullptr)
        throw INVALID_ARGUMENT("Socket address of publisher wasn't specified");
    runPar.pubAddr = SockAddr(argv[optind++]);

    if (argv[optind] == nullptr)
        throw INVALID_ARGUMENT("IP address for local P2P server wasn't specified");
    runPar.p2p.srvr.addr = SockAddr(argv[optind++], 0);

    if (optind != argc)
        throw INVALID_ARGUMENT("Excess arguments were specified");

    vetRunPars();
    init(); // Initializes non-command-line variables
}

static void getSubInfo()
{
    /**
     * Gets subscriber information from the publisher. Connects to it, sends the socket address
     * of the local P2P server, reads the subscription information, and closes the connection.
     *
     * @param[in]  pubAddr  Socket address of the publisher
     * @param[out] subInfo  Subscription information
     */
    void getSubInfo(
            const SockAddr pubAddr,
            SubInfo&       subInfo) {
        // Keep consonant with `Publisher::serve()`
        Xprt xprt{TcpClntSock(pubAddr)};
        p2pMgr->getSrvrAddr().write(xprt);
        subInfo.read(xprt);
    }
}

/**
 * Halts the subscribing node.
 *
 * @param[in] sig  Signal number. Ignored.
 */
static void sigHand(const int sig)
{
    subNode->halt(); // Gracefully terminate
}

/**
 * Sets signal handler.
 */
static void setSigHand()
{
    log_setLevelSignal(SIGUSR2);

    struct sigaction sigact;
    (void) sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0; // Allow SIGINT and SIGTERM to interrupt system calls
    sigact.sa_handler = &sigHand;
    (void)sigaction(SIGINT, &sigact, NULL);
    (void)sigaction(SIGTERM, &sigact, NULL);

    // Ensure that the above signals will be caught
    sigset_t sigset;
    (void)sigemptyset(&sigset);
    (void)sigaddset(&sigset, SIGINT);
    (void)sigaddset(&sigset, SIGTERM);
    (void)sigprocmask(SIG_UNBLOCK, &sigset, NULL);
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
    int status;

    std::set_terminate(&terminate); // NB: Hycast version

    try {
        getCmdPars(argc, argv);
        getSubInfo();

        subNode = SubNode::create(
            runPar.pubAddr,
            runPar.mcastIface,

            runPar.p2p.srvr.addr,
            runPar.p2p.srvr.listenSize,
            runPar.p2p.maxPeers,
            runPar.p2p.evalTime,

            runPar.p2p.trackerSize,

            runPar.repo.rootDir,
            runPar.repo.maxOpenFiles);

        setSigHand(); // Catches termination signals
    }
    catch (const std::invalid_argument& ex) {
        LOG_FATAL(ex);
        usage();
        status = 1;
    }
    catch (const std::exception& ex) {
        LOG_FATAL(ex);
        status = 2;
    }

    return 0;
}
