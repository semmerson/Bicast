/**
 * Program to publish data-products via Bicast.
 *
 *        File: publish.cpp
 *  Created on: Aug 13, 2020
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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

#include "config.h"

#include "logging.h"
#include "Node.h"
#include "P2pSrvrInfo.h"
#include "Parser.h"
#include "SubInfo.h"
#include "ThreadException.h"

#include <inttypes.h>
#include <semaphore.h>
#include <thread>
#include <yaml-cpp/yaml.h>

using namespace bicast;

using String = std::string;

/// Default maximum number of active peers
static constexpr int  DEF_MAX_PEERS     = 8;
/// Size of queue for publisher's server
static constexpr int  DEF_BACKLOG_SIZE  = 256;
/// Default multicast group Internet address
static constexpr char DEF_MCAST_ADDR[]  = "232.1.1.1";
/// Default port for publisher's server & multicast group
static constexpr int  DEF_PORT          = 38800;
/// Default tracker size
static constexpr int  DEF_TRACKER_CAP   = 1000;
/// Default peer evaluation duration
static constexpr int  DEF_EVAL_DURATION = 300;

/// Command-line/configuration-file parameters of this program
struct RunPar {
    String    feedName;   ///< Name of data-product stream
    LogLevel  logLevel;   ///< Logging level
    int32_t   maxSegSize; ///< Maximum size of a data-segment in bytes
    int       trackerCap; ///< Maximum size of pool of potential P2P servers
    String    pubRoot;    ///< Pathname of the publisher's root directory
    /// Runtime parameters for a publisher's server (not its P2P server)
    struct Srvr {
        SockAddr      addr;               ///< Socket address of publisher's server (not P2P server)
        int           listenSize;         ///< Size of `listen()` queue
        /**
         * Constructs.
         * @param addr        Socket address of publisher's server (not P2P server)
         * @param listenSize  Size of publisher's `listen()` queue
         */
        Srvr(   const SockAddr addr,
                const int      listenSize)
            : addr(addr)
            , listenSize{listenSize}
        {}
    }                     srvr;           ///< Publisher's server
    McastPub::RunPar      mcast;          ///< Multicast component
    PubP2pMgr::RunPar     p2p;            ///< Peer-to-peer component
    PubNode::RunPar::Repo repo;           ///< Runtime parameters for the publisher's repository

    /**
     * Default constructs.
     */
    RunPar()
        : feedName("Bicast")
        , logLevel(LogLevel::NOTE)
        , maxSegSize(1444)
        , trackerCap(DEF_TRACKER_CAP)
        , pubRoot(".")
        , srvr(SockAddr("0.0.0.0", DEF_PORT), DEF_BACKLOG_SIZE)
        , mcast(SockAddr(DEF_MCAST_ADDR, DEF_PORT), InetAddr())
        , p2p(SockAddr(), DEF_MAX_PEERS, DEF_MAX_PEERS, DEF_EVAL_DURATION)
        , repo(::sysconf(_SC_OPEN_MAX)/2, 3600)
    {}
};

/// Helper class for counting and throttling the number of threads handling subscriptions.
class Counter {
    mutable Mutex mutex;          ///< Count mutex
    mutable Cond  cond;           ///< Count condition variable
    int           max;            ///< Maximum count allowed
    int           count;          ///< Current count
public:
    Counter()
        : mutex()
        , cond()
        , max(DEF_MAX_PEERS)
        , count(0)
    {}
    /**
     * Sets the maximum allowable count.
     * @param[in] max  The maximum allowable count
     */
    void setMax(const int max) {
        this->max = max;
    }
    /**
     * Increments the count. Blocks until the count is less than the maximum allowable.
     */
    void waitToInc() {
        Lock lock{mutex};
        LOG_DEBUG("count=" + std::to_string(count) + "; max=" + std::to_string(max));
        cond.wait(lock, [&]{return count < max;});
        ++count;
    }
    /**
     * Decrements the count.
     */
    void operator--() {
        Guard guard{mutex};
        --count;
        cond.notify_all();
    }
}                        numSubThreads; ///< Subscriber thread count
static sem_t             stopSem;       ///< Semaphore for async-signal-safe stopping
static std::atomic<bool> stop;          ///< Should the program stop?
static ThreadEx          threadEx;      ///< Exception thrown by a thread
static RunPar            runPar;        ///< Runtime parameters:
static const RunPar      defRunPar;     ///< Default runtime parameters
static PubNodePtr        pubNode;       ///< Data-product publishing node
static SubInfo           subInfo;       ///< Subscription information passed to subscribers
static Tracker           tracker;       ///< Tracks P2P servers

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << log_getName() << " -h\n"
"    " << log_getName() << " [options]\n"
"Options:\n"
"  General:\n"
"    -c <configFile>   Pathname of configuration-file. Overrides previous\n"
"                      arguments; overridden by subsequent ones.\n"
"    -d <maxSegSize>   Maximum data-segment size in bytes. Default is " <<
                       defRunPar.maxSegSize << ".\n"
"    -f <name>         Name of data-product feed. Default is \"" << defRunPar.feedName << "\".\n"
"    -h                Print this help message on standard error, then exit.\n"
"    -l <logLevel>     Logging level. <level> is \"FATAL\", \"ERROR\", \"WARN\",\n"
"                      \"NOTE\", \"INFO\", \"DEBUG\", or \"TRACE\". Comparison is case-\n"
"                      insensitive and takes effect immediately. Default is\n" <<
"                      \"" << defRunPar.logLevel << "\".\n"
"    -r <pubRoot>      Pathname of publisher's root-directory. Default is \"" << runPar.pubRoot <<
                       "\".\n"
"    -t <trackerCap>   Maximum number of P2P servers to track. Default is " <<
                       defRunPar.trackerCap << ".\n"
"  Publisher's Server:\n"
"    -P <pubAddr>      Socket address of publisher's server (not the P2P server).\n"
"                      Default is \"" << defRunPar.srvr.addr << "\".\n"
"    -Q <maxPending>   Maximum number of pending connections to publisher's\n"
"                      server (not the P2P server). Default is " << defRunPar.srvr.listenSize <<
                       ".\n"
"  Multicasting:\n"
"    -m <dstAddr>      Destination address of multicast group. Default is\n" <<
"                      \"" << defRunPar.mcast.dstAddr << "\".\n"
"    -s <srcAddr>      Internet address of multicast source/interface. Must not\n"
"                      be wildcard. Default is determined by operating system\n"
"                      based on destination address of multicast group.\n"
"  Peer-to-Peer:\n"
"    -e <evalTime>     Peer evaluation duration, in seconds, before replacing\n"
"                      poorest performer. Default is " << defRunPar.p2p.evalTime << ".\n"
"    -n <maxPeers>     Maximum number of connected peers. Default is " <<
                       defRunPar.p2p.maxPeers << ".\n"
"    -p <p2pAddr>      Internet address for local P2P server (not the publisher's\n"
"                      server). Must not be wildcard. Default is IP address of\n"
"                      interface used for multicasting.\n"
"    -q <maxPending>   Maximum number of pending connections to P2P server (not\n"
"                      the publisher's server). Default is " << defRunPar.p2p.srvr.acceptQSize <<
                       ".\n"
"  Repository:\n"
"    -k <keepTime>     How long to keep data-products in seconds. Default is\n" <<
"                      " << defRunPar.repo.keepTime << ".\n"
"    -o <maxOpenFiles> Maximum number of open repository files. Default is " <<
                       defRunPar.repo.maxOpenFiles << ".\n"
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
        Parser::tryDecode<decltype(runPar.pubRoot)>(node0, "pubRoot", runPar.pubRoot);
        Parser::tryDecode<decltype(runPar.feedName)>(node0, "name", runPar.feedName);
        auto node1 = node0["logLevel"];
        if (node1)
            log_setLevel(node1.as<String>());
        Parser::tryDecode<decltype(runPar.maxSegSize)>(node0, "maxSegSize", runPar.maxSegSize);
        Parser::tryDecode<decltype(runPar.trackerCap)>(node0, "trackerCap",
                runPar.trackerCap);

        node1 = node0["server"];
        if (node1) {
            auto node2 = node1["pubAddr"];
            if (node2)
                runPar.srvr.addr = SockAddr(node2.as<String>());
            Parser::tryDecode<decltype(runPar.srvr.listenSize)>(node1, "maxPending",
                    runPar.srvr.listenSize);
        }

        node1 = node0["multicast"];
        if (node1) {
            auto node2 = node1["dstAddr"];
            if (node2)
                runPar.mcast.dstAddr = SockAddr(node2.as<String>());
            node2 = node1["srcAddr"];
            if (node2)
                runPar.mcast.srcAddr = InetAddr(node2.as<String>());
        }

        node1 = node0["peer2Peer"];
        if (node1) {
            auto node2 = node1["server"];
            if (node2) {
                auto node3 = node2["p2pAddr"];
                if (node3)
                    runPar.p2p.srvr.addr = SockAddr(node3.as<String>(), 0);
                Parser::tryDecode<decltype(runPar.p2p.srvr.acceptQSize)>(node2, "maxPending",
                        runPar.p2p.srvr.acceptQSize);
            }

            Parser::tryDecode<decltype(runPar.p2p.maxPeers)>(node1, "maxPeers",
                    runPar.p2p.maxPeers);
            Parser::tryDecode<decltype(runPar.p2p.evalTime)>(node1, "evalTime",
                    runPar.p2p.evalTime);
        }

        node1 = node0["repository"];
        if (node1) {
            Parser::tryDecode<decltype(runPar.repo.maxOpenFiles)>(node1, "maxOpenFiles",
                    runPar.repo.maxOpenFiles);
            Parser::tryDecode<decltype(runPar.repo.keepTime)>(node1, "keepTime",
                    runPar.repo.keepTime);
        }
    } // YAML file loaded
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse YAML file \"" +
                pathname + "\""));
    }
}

/// Vets the command-line/configuration-file parameters.
static void vetRunPar()
{
    if (runPar.mcast.dstAddr.getInetAddr().getFamily() !=
            runPar.mcast.srcAddr.getFamily())
        throw INVALID_ARGUMENT("Address of multicast group (" + runPar.mcast.dstAddr.to_string() +
                ") and address of multicast source (" + runPar.mcast.srcAddr.to_string() +
                ") belong to different address families");

    if (runPar.p2p.evalTime <= 0)
        throw INVALID_ARGUMENT("Peer performance evaluation-time is not positive");

    if (runPar.trackerCap <= 0)
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
        throw INVALID_ARGUMENT("Size of publisher's server-queue is not positive");

    if (runPar.p2p.srvr.acceptQSize <= 0)
        throw INVALID_ARGUMENT("Size of P2P server-queue is not positive");

    if (runPar.pubRoot.empty())
        throw INVALID_ARGUMENT("Name of publisher's root-directory is the empty string");

    if (runPar.maxSegSize <= 0)
        throw INVALID_ARGUMENT("Maximum size of a data-segment is not positive");
    if (runPar.maxSegSize > UdpSock::MAX_PAYLOAD)
        throw INVALID_ARGUMENT("Maximum size of a data-segment (" +
                std::to_string(runPar.maxSegSize) + ") is greater than UDP maximum (" +
                std::to_string(UdpSock::MAX_PAYLOAD) + ")");
}

/// Initializes runtime parameters that aren't set from the command-line/configuration-file
static void initRunPar()
{
    DataSeg::setMaxSegSize(runPar.maxSegSize);

    subInfo.version = 1;
    subInfo.feedName = runPar.feedName;
    subInfo.maxSegSize = runPar.maxSegSize;
    subInfo.mcast.dstAddr = runPar.mcast.dstAddr;
    subInfo.mcast.srcAddr = runPar.mcast.srcAddr;
    subInfo.tracker = Tracker(runPar.trackerCap);
    subInfo.keepTime = runPar.repo.keepTime;

    numSubThreads.setMax(runPar.p2p.maxPeers);
}

/**
 * Sets command-line/configuration-file parameters.
 *
 * @param[in] argc               Number of command-line arguments
 * @param[in] argv               Command-line arguments
 * @throw std::invalid_argument  Invalid option, option argument, or operand
 * @throw std::logic_error       Too many or too few operands
 */
static void getCmdPars(
        const int    argc,
        char* const* argv)
{
    runPar = defRunPar;

    opterr = 0;    // 0 => getopt() won't write to `stderr`
    int c;
    while ((c = ::getopt(argc, argv, ":c:d:e:f:hk:l:m:o:P:p:Q:q:r:s:t:")) != -1) {
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
        case 'd': {
            int maxSegSize;
            if (::sscanf(optarg, "%d", &maxSegSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            runPar.maxSegSize = maxSegSize;
            break;
        }
        case 'e': {
            int evalTime;
            if (::sscanf(optarg, "%d", &evalTime) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            runPar.p2p.evalTime = evalTime;
            break;
        }
        case 'f': {
            runPar.feedName = String(optarg);
            break;
        }
        case 'k': {
            int keepTime;
            if (::sscanf(optarg, "%" SCNd32, &keepTime) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            runPar.repo.keepTime = keepTime;
            break;
        }
        case 'l': {
            log_setLevel(optarg);
            runPar.logLevel = log_getLevel();
            break;
        }
        case 'm': {
            runPar.mcast.dstAddr = SockAddr(optarg);
            break;
        }
        case 'n': {
            int maxPeers;
            if (::sscanf(optarg, "%d", &maxPeers) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            runPar.p2p.maxPeers = maxPeers;
            break;
        }
        case 'o': {
            if (::sscanf(optarg, "%ld", &runPar.repo.maxOpenFiles) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            break;
        }
        case 'P': {
            runPar.srvr.addr = SockAddr(optarg);
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
            break;
        }
        case 'q': {
            if (::sscanf(optarg, "%d", &runPar.p2p.srvr.acceptQSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            break;
        }
        case 'r': {
            runPar.pubRoot = String(optarg);
            break;
        }
        case 's': {
            runPar.mcast.srcAddr = InetAddr(optarg);
            break;
        }
        case 't': {
            int trackerCap;
            if (::sscanf(optarg, "%d", &trackerCap) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            runPar.trackerCap = trackerCap;
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

    if (!runPar.mcast.srcAddr)
        runPar.mcast.srcAddr = UdpSock(runPar.mcast.dstAddr).getLclAddr().getInetAddr();

    if (!runPar.p2p.srvr.addr)
        runPar.p2p.srvr.addr = SockAddr(runPar.mcast.srcAddr, 0);

    vetRunPar();
}

/**
 * Handles a termination signal.
 *
 * @param[in] sig  Signal number. Ignored.
 */
static void sigHandler(const int sig)
{
    ::sem_post(&stopSem);
}

/**
 * Sets signal handling.
 */
static void setSigHandling()
{
    log_setLevelSignal(SIGUSR2);

    struct sigaction sigact = {};
    (void) sigemptyset(&sigact.sa_mask);
    /*
     * System calls interrupted by a termination signal are restarted because termination is more
     * cleanly handled by this application.
     *
     * Bullshit! The system calls *should* be interrupted! Such calls should be restarted only for
     * innocuous signals like ones that print metrics or reset logging.
    sigact.sa_flags = SA_RESTART;
     */
    sigact.sa_handler = &sigHandler;
    (void)sigaction(SIGINT, &sigact, NULL);
    (void)sigaction(SIGTERM, &sigact, NULL);
}

/// Sets the exception thrown on an internal thread to the current exception.
static void setException()
{
    threadEx.set();
    ::sem_post(&stopSem);
}

/// Runs the publishing node. Meant to be the start routine for a thread.
static void runNode()
{
    try {
        pubNode->run();
    }
    catch (const std::exception& ex) {
        //LOG_DEBUG("Setting exception: %s", ex.what());
        setException();
    }
}

/**
 * Handles a subscription request. Meant to be the start routine for a thread.
 *
 * @param[in] xprt  Transport connected to the subscriber
 */
static void servSubscriber(Xprt xprt)
{
    try {
        // Keep consonant with `subscribe.cpp:subscribe()`

        Tracker     subTracker{subInfo.tracker.getCapacity()}; // Subscriber's tracker
        P2pSrvrInfo subP2pSrvrInfo; // Information on subscriber's P2P server

        if (!subP2pSrvrInfo.read(xprt))
            throw RUNTIME_ERROR("Couldn't receive information on P2P server from subscriber " +
                    xprt.getRmtAddr().to_string());
        LOG_INFO("Received information on P2P server " + subP2pSrvrInfo.to_string() +
                " from subscriber " + xprt.getRmtAddr().to_string());

        if (!subTracker.read(xprt))
            throw RUNTIME_ERROR("Couldn't receive tracker from subscriber " +
                    xprt.getRmtAddr().to_string());
        LOG_INFO("Received tracker " + subTracker.to_string() + " from subscriber" +
                xprt.getRmtAddr().to_string());

        // Ensure that the publisher's tracker contains current information on the publisher's P2P
        // server
        subInfo.tracker.insert(pubNode->getP2pSrvrInfo());

        if (!subInfo.write(xprt))
            throw RUNTIME_ERROR("Couldn't send subscription information to subscriber " +
                    xprt.getRmtAddr().to_string());
        LOG_INFO("Sent subscription information to subscriber " + xprt.getRmtAddr().to_string());

        subInfo.tracker.insert(subTracker);
        subInfo.tracker.insert(subP2pSrvrInfo);

        --numSubThreads;
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex, "Couldn't serve subscriber %s", xprt.getRmtAddr().to_string().data());
        --numSubThreads;
    }
    catch (...) {
        --numSubThreads;
        throw;
    }
}

/// Serves subscribers.
static void runServer()
{
    try {
        //LOG_DEBUG("Creating listening server");
        auto srvrSock = TcpSrvrSock(runPar.srvr.addr, runPar.srvr.listenSize);
        LOG_NOTE("Created publisher's server " + srvrSock.to_string());

        for (;;) {
            numSubThreads.waitToInc();
            auto xprt = Xprt(srvrSock.accept()); // RAII
            LOG_NOTE("Accepted connection from subscriber " + xprt.getRmtAddr().to_string());
            std::thread(&servSubscriber, xprt).detach();
        }
    } catch (const std::exception& ex) {
        //LOG_DEBUG("Setting exception: %s", ex.what());
        setException();
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

    std::set_terminate(&terminate); // NB: Bicast version

    try {
        log_setName(::basename(argv[0]));
        LOG_NOTE("Starting up: " + getCmdLine(argc, argv));

        getCmdPars(argc, argv);
        initRunPar();

        if (::sem_init(&stopSem, 0, 0) == -1)
                throw SYSTEM_ERROR("Couldn't initialize semaphore");

        setSigHandling(); // Catches termination signals

        tracker = Tracker(runPar.trackerCap); // Create the tracker

        //LOG_DEBUG("Starting server thread");
        auto serverThread = Thread(&runServer);

        pubNode = PubNode::create(tracker, runPar.maxSegSize, runPar.mcast, runPar.p2p,
                runPar.pubRoot, runPar.repo, runPar.feedName);
        //LOG_DEBUG("Starting node thread");
        auto nodeThread = Thread(&runNode);

        //LOG_DEBUG("Waiting on semaphore");
        ::sem_wait(&stopSem); // Returns if failure on a thread or termination signal
        ::sem_destroy(&stopSem);

        //LOG_DEBUG("Canceling server thread");
        ::pthread_cancel(serverThread.native_handle());
        //LOG_DEBUG("Halting node");
        pubNode->halt(); // Idempotent

        //LOG_DEBUG("Joining server thread");
        serverThread.join();
        //LOG_DEBUG("Joining node thread");
        nodeThread.join();

        //LOG_DEBUG("Throwing exception if set");
        threadEx.throwIfSet(); // Throws if failure on a thread

        status = 0;
    }
    catch (const std::invalid_argument& ex) {
        LOG_FATAL(ex);
        usage();
        status = 1;
    }
    catch (const std::exception& ex) {
        LOG_FATAL(ex.what());
        status = 2;
    }
    LOG_NOTE("Exiting with status %d", status);

    return status;
}
