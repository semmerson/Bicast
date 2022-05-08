/**
 * Program to publish data-products via Hycast.
 *
 *        File: publisher.cpp
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

#include "error.h"
#include "HycastProto.h"
#include "Node.h"
#include "SockAddr.h"
#include "ThreadException.h"

#include <atomic>
#include <cstring>
#include <cstdio>
#include <exception>
#include <inttypes.h>
#include <iostream>
#include <limits.h>
#include <semaphore.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

using namespace hycast;

using String = std::string;

/// Runtime parameters of this program
struct RunPar {
    String    feedName;                   ///< Name of data-product stream
    LogLevel  logLevel;                   ///< Logging level
    int32_t   maxSegSize;                 ///< Maximum size of a data-segment in bytes
    struct Srvr {
        SockAddr  addr;                   ///< Socket address
        int       listenSize;             ///< Size of `::listen()` queue
        Srvr(   const SockAddr addr,
                const int      listenSize)
            : addr(addr)
            , listenSize{listenSize}
        {}
    }         srvr;                       ///< Publisher's server
    McastPub::RunPar  mcast;              ///< Multicast component
    PubP2pMgr::RunPar p2p;                ///< Peer-to-peer component
    PubRepo::RunPar   repo;               ///< Data-product repository
    RunPar()
        : feedName("Hycast")
        , logLevel(LogLevel::NOTE)
        , maxSegSize(1444)
        , srvr(SockAddr("0.0.0.0:38800"), 256)
        , mcast(SockAddr("232.1.1.1:38800"), InetAddr())
        , p2p(SockAddr(), 8, 8, 100, 60000)
        , repo("repo", maxSegSize, ::sysconf(_SC_OPEN_MAX)/2, 3600)
    {
        mcast.srcAddr = UdpSock(mcast.dstAddr).getLclAddr().getInetAddr();
        p2p.srvr.addr = mcast.srcAddr.getSockAddr(0);
    }
};

// Subscription information transmitted to each subscriber
struct SubInfo : public XprtAble {
    uint16_t  version;    ///< Protocol version
    String    feedName;   ///< Name of data-product stream
    SegSize   maxSegSize; ///< Maximum size of a data-segment in bytes
    struct Mcast {
        SockAddr dstAddr; ///< Multicast destination address
        InetAddr srcAddr; ///< Multicast source address
    }         mcast;
    struct P2p {
        SockAddr addr;    ///< Address of publisher's P2P server
        Tracker  tracker; ///< Addresses of subscriber's P2P servers
    }         p2p;
    uint32_t  keepTime;   ///< Duration to keep data-products in seconds
    bool write(Xprt xprt) const {
        return xprt.write(version) &&
                xprt.write<uint8_t>(feedName) &&
                xprt.write(maxSegSize) &&
                mcast.dstAddr.write(xprt) &&
                mcast.srcAddr.write(xprt) &&
                p2p.addr.write(xprt) &&
                p2p.tracker.write(xprt) &&
                xprt.write(keepTime);
    }
    bool read(Xprt xprt) {
        return xprt.read(version) &&
                xprt.read<uint8_t>(feedName) &&
                xprt.read(maxSegSize) &&
                mcast.dstAddr.read(xprt) &&
                mcast.srcAddr.read(xprt) &&
                p2p.addr.read(xprt) &&
                p2p.tracker.read(xprt) &&
                xprt.read(keepTime);
    }
};

static sem_t             sem;       ///< Semaphore for state changes
static std::atomic<bool> stop;      ///< Should the program stop?
static ThreadEx          threadEx;  ///< Exception thrown by a thread
static RunPar            runPar;    ///< Runtime parameters:
const static RunPar      defRunPar; ///< Default runtime parameters
static PubNode::Pimpl    pubNode{}; ///< Data-product publishing node
static SubInfo           subInfo;   ///< Subscription information passed to subscribers
static Tracker           tracker;   ///< Addresses of subscriber's P2P servers

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << log_getName() << " -h\n"
"    " << log_getName() << "[-c <configFile>] [-e <evalTime>] [-f <name>] [-i <pubAddr>]\n"
"        [-k <keepTime>] [-L <trackerSize>] [-l <level>] [-M <mcastAddr>]\n"
"        [-m <maxPeers>] [-o <maxOpenFiles>] [-p <p2pAddr>] [-Q <listenSize>]\n"
"        [-q <listenSize>] [-r <repoRoot>] [-S <srcAddr>] [-s <maxSegSize>]\n"
"where:\n"
"    -h                Print this help message on standard error, then exit.\n"
"\n"
"    -c <configFile>   Pathname of configuration-file. Overrides previous\n"
"                      arguments; overridden by subsequent ones.\n"
"    -e <evalTime>     Peer evaluation duration, in ms, before replacing poorest\n"
"                      performer. Default is " << defRunPar.p2p.evalTime << ".\n"
"    -f <name>         Name of data-product feed. Default is \"" << defRunPar.feedName << "\".\n"
"    -i <pubAddr>      Socket address of the publisher (not the P2P server).\n"
"                      Default is \"" << defRunPar.srvr.addr << "\".\n"
"    -k <keepTime>     How long to keep data-products in seconds. Default is\n" <<
"                      " << defRunPar.repo.keepTime << " s.\n"
"    -L <trackerSize>  Maximum size of the list of remote P2P servers. Default is\n" <<
"                      " << defRunPar.p2p.trackerSize << ".\n"
"    -l <level>        Logging level. <level> is \"FATAL\", \"ERROR\", \"WARN\",\n"
"                      \"NOTE\", \"INFO\", \"DEBUG\", or \"TRACE\". Comparison is case-\n"
"                      insensitive and takes effect immediately. Default is\n" <<
"                      \"" << defRunPar.logLevel << "\".\n"
"    -M <mcastAddr>    Destination address of multicast group. Default is\n" <<
"                      \"" << defRunPar.mcast.dstAddr << "\".\n"
"    -m <maxPeers>     Maximum number of connected peers. Default is " <<
                       defRunPar.p2p.maxPeers << ".\n"
"    -o <maxOpenFiles> Maximum number of open repository files. Default is " <<
                       defRunPar.repo.maxOpenFiles << ".\n"
"    -p <p2pAddr>      Internet address of local P2P server (not the publisher).\n"
"                      Default is \"" << defRunPar.p2p.srvr.addr.getInetAddr() << "\".\n"
"    -Q <listenSize>   Size of publisher's listen() queue (not the P2P server's).\n"
"                      Default is " << defRunPar.srvr.listenSize << ".\n"
"    -q <listenSize>   Size of P2P server's listen() queue (not the publisher's).\n"
"                      Default is " << defRunPar.p2p.srvr.listenSize << ".\n"
"    -r <repoRoot>     Pathname of root of publisher's repository. Default is\n"
"                      \"" << defRunPar.repo.rootDir << "\".\n"
"    -S <srcAddr>      Internet address of multicast source (i.e., multicast\n"
"                      interface). Default is \"" << defRunPar.mcast.srcAddr << "\".\n"
"    -s <maxSegSize>   Maximum size of a data-segment in bytes. Default is " <<
                       defRunPar.maxSegSize << ".\n"
"SIGUSR2 rotates the logging level.\n";
}

static void vetRunPar()
{
    if (runPar.p2p.evalTime <= 0)
        throw INVALID_ARGUMENT("Peer performance evaluation-time is not positive");

    if (runPar.p2p.trackerSize <= 0)
        throw INVALID_ARGUMENT("Tracker size is not positive");

    if (runPar.p2p.maxPeers <= 0)
        throw INVALID_ARGUMENT("Maximum number of peers is not positive");

    if (runPar.repo.maxOpenFiles <= 0)
        throw INVALID_ARGUMENT("Maximum number of open repository files is not positive");
    if (runPar.repo.maxOpenFiles > sysconf(_SC_OPEN_MAX))
        throw INVALID_ARGUMENT("Maximum number of open repository files is "
                "greater than system maximum, " + std::to_string(sysconf(_SC_OPEN_MAX)));
    if (runPar.repo.keepTime <= 0)
        throw INVALID_ARGUMENT("How long to keep repository files is not positive");

    if (runPar.srvr.listenSize <= 0)
        throw INVALID_ARGUMENT("Size of publisher's listen() queue is not positive");

    if (runPar.p2p.srvr.listenSize <= 0)
        throw INVALID_ARGUMENT("Size of P2P server's listen() queue is not positive");

    if (runPar.repo.rootDir.empty())
        throw INVALID_ARGUMENT("Name of repository's root-directory is the empty string");

    if (runPar.maxSegSize <= 0)
        throw INVALID_ARGUMENT("Maximum size of a data-segment is not positive");
    if (runPar.maxSegSize > UdpSock::MAX_PAYLOAD)
        throw INVALID_ARGUMENT("Maximum size of a data-segment (" +
                std::to_string(runPar.maxSegSize) + ") is greater than UDP maximum (" +
                std::to_string(UdpSock::MAX_PAYLOAD) + ")");
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

        tryDecode<decltype(runPar.feedName)>(node0, "Name", runPar.feedName);
        tryDecode<decltype(runPar.maxSegSize)>(node0, "MaxSegSize", runPar.maxSegSize);

        node1 = node0["Server"];
        if (node1) {
            auto node2 = node1["SockAddr"];
            if (node2)
                runPar.srvr.addr = SockAddr(node2.as<String>());

            tryDecode<decltype(runPar.srvr.listenSize)>(node1, "ListenSize",
                    runPar.srvr.listenSize);
        }

        node1 = node0["Node"];
        if (node1) {
            auto node2 = node1["Multicast"];
            if (node2) {
                auto node3 = node2["GroupAddr"];
                if (node3)
                    runPar.mcast.dstAddr = SockAddr(node3.as<String>());

                node3 = node2["Source"];
                if (node3)
                    runPar.mcast.srcAddr = InetAddr(node3.as<String>());
            }

            node2 = node1["Peer2Peer"];
            if (node2) {
                auto node3 = node2["Server"];
                if (node3) {
                    auto node3 = node2["InetAddr"];
                    if (node3)
                        runPar.p2p.srvr.addr = SockAddr(node3.as<String>(), 0);

                    tryDecode<decltype(runPar.p2p.srvr.listenSize)>(node3, "ListenSize",
                            runPar.p2p.srvr.listenSize);
                }

                tryDecode<decltype(runPar.p2p.maxPeers)>(node2, "MaxPeers", runPar.p2p.maxPeers);
                tryDecode<decltype(runPar.p2p.trackerSize)>(node2, "TrackerSize",
                        runPar.p2p.trackerSize);
            }

            node2 = node1["ReplaceTrigger"];
            if (node2) {
                auto node3 = node2["Type"];
                if (node3 && node3.as<String>() == "time")
                    tryDecode<decltype(runPar.p2p.evalTime)>(node2, "Duration",
                            runPar.p2p.evalTime);
            }

            node2 = node1["Repository"];
            if (node2) {
                tryDecode<decltype(runPar.repo.rootDir)>(node2, "Pathname", runPar.repo.rootDir);
                tryDecode<decltype(runPar.repo.maxOpenFiles)>(node2, "MaxOpenFiles",
                        runPar.repo.maxOpenFiles);
                tryDecode<decltype(runPar.repo.keepTime)>(node2, "KeepTime", runPar.repo.keepTime);
            }
        }
    } // YAML file loaded
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse YAML file \"" +
                pathname + "\""));
    }
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
    runPar = defRunPar;

    opterr = 0;    // 0 => getopt() won't write to `stderr`
    int c;
    while ((c = ::getopt(argc, argv, ":c:e:f:hi:L:l:M:m:o:p:Q:q:r:S:s:y:")) != -1) {
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
        case 'f': {
            runPar.feedName = String(optarg);
            if (runPar.feedName.empty())
                throw INVALID_ARGUMENT("Feed name is the empty string");
            break;
        }
        case 'i': {
            runPar.srvr.addr = SockAddr(optarg);
            break;
        }
        case 'k': {
            int keepTime;
            if (::sscanf(optarg, "%" SCNd32, &keepTime) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (keepTime <= 0)
                throw INVALID_ARGUMENT("How long to keep data-products is not positive");
            runPar.repo.keepTime = keepTime;
            break;
        }
        case 'L': {
            int trackerSize;
            if (::sscanf(optarg, "%d", &trackerSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (trackerSize <= 0)
                throw INVALID_ARGUMENT("Tracker size is not positive");
            runPar.p2p.trackerSize = trackerSize;
            break;
        }
        case 'l': {
            log_setLevel(optarg);
            runPar.logLevel = log_getLevel();
            break;
        }
        case 'M': {
            runPar.mcast.dstAddr = SockAddr(optarg);
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
        case 'p': {
            runPar.p2p.srvr.addr = SockAddr(optarg, 0);
            break;
        }
        case 'Q': {
            if (::sscanf(optarg, "%d", &runPar.srvr.listenSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (runPar.srvr.listenSize <= 0)
                throw INVALID_ARGUMENT("Size of publisher's listen() queue is not positive");
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
        case 'S': {
            runPar.mcast.srcAddr = InetAddr(optarg);
            break;
        }
        case 's': {
            int maxSegSize;
            if (::sscanf(optarg, "%d", &maxSegSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (maxSegSize <= 0)
                throw INVALID_ARGUMENT("Maximum size of a data-segment is not positive");
            runPar.maxSegSize = maxSegSize;
            break;
        }
        case ':': { // Missing option argument. Due to leading ":" in opt-string
            throw INVALID_ARGUMENT(String("Option \"-") + static_cast<char>(optopt) +
                    "\" is missing an argument");
        }
        default : { // c == '?'
            throw INVALID_ARGUMENT(String("Unknown \"-") + static_cast<char>(optopt) + "\" option");
        }
        } // `switch` statement
    } // While getopt() loop

    if (optind != argc)
        throw LOGIC_ERROR("Too many operands specified");

    vetRunPar();
    DataSeg::setMaxSegSize(runPar.maxSegSize);
}

static void init()
{
    if (sem_init(&sem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");

    tracker = Tracker(runPar.p2p.trackerSize);

    subInfo.version = 1;
    subInfo.feedName = runPar.feedName;
    subInfo.maxSegSize = runPar.maxSegSize;
    subInfo.mcast.dstAddr = runPar.mcast.dstAddr;
    subInfo.mcast.srcAddr = runPar.mcast.srcAddr;
    subInfo.p2p.addr = runPar.p2p.srvr.addr;
    subInfo.p2p.tracker = tracker;
    subInfo.keepTime = runPar.repo.keepTime;
}

/**
 * Signals the program to halt.
 */
static void halt()
{
    sem_post(&sem);
}

/**
 * Handles a termination signal.
 *
 * @param[in] sig  Signal number. Ignored.
 */
static void sigHandler(const int sig)
{
    halt();
}

/**
 * Sets signal handling.
 */
static void setSigHandling()
{
    log_setLevelSignal(SIGUSR2);

    struct sigaction sigact;
    (void) sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART; // Restart system calls if interrupted by a termination signal
    sigact.sa_handler = &sigHandler;
    (void)sigaction(SIGINT, &sigact, NULL);
    (void)sigaction(SIGTERM, &sigact, NULL);
}

static void setException(const std::exception& ex)
{
    threadEx.set(ex);
    halt();
}

static void runNode()
{
    try {
        pubNode->run();
    }
    catch (const std::exception& ex) {
        setException(ex);
    }
}

static void serve()
{
    try {
        auto srvrSock = TcpSrvrSock(runPar.srvr.addr, runPar.srvr.listenSize);
        for (;;) {
            auto xprt = Xprt(srvrSock.accept());
            SockAddr p2pSrvrAddr;
            if (p2pSrvrAddr.read(xprt) && subInfo.write(xprt))
                tracker.insert(p2pSrvrAddr);
        }
    } catch (const std::exception& ex) {
        setException(ex);
    }
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
    int status;

    std::set_terminate(&terminate); // NB: Hycast version

    try {
        getRunPars(argc, argv);
        init(); // Initializes non-command-line variables

        pubNode = PubNode::create(runPar.maxSegSize, runPar.mcast, runPar.p2p, runPar.repo);
        setSigHandling(); // Catches termination signals

        auto nodeThread = Thread(&runNode);
        auto serverThread = Thread(&serve);

        sem_wait(&sem); // Returns if thread failure or termination signal
        sem_destroy(&sem);
        threadEx.throwIfSet(); // Throws if thread failure

        ::pthread_cancel(serverThread.native_handle());
        pubNode->halt(); // Gracefully terminates

        serverThread.join();
        nodeThread.join();

        status = 0;
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
    LOG_NOTE("Exiting with status %d", status);

    return status;
}
