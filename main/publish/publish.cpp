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
#include "RunPar.h"
#include "SubInfo.h"
#include "ThreadException.h"

#include <inttypes.h>
#include <semaphore.h>
#include <thread>
#include <yaml-cpp/yaml.h>

using namespace bicast;

/// Helper class for counting and throttling the number of threads handling subscriptions.
class Counter {
    mutable Mutex mutex; ///< Count mutex
    mutable Cond  cond;  ///< Count condition variable
    int           max;   ///< Maximum count allowed
    int           count; ///< Current count
public:
    Counter()
        : mutex()
        , cond()
        , max(8)
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
static PubNodePtr        pubNode;       ///< Data-product publishing node
static SubInfo           subInfo;       ///< Subscription information passed to subscribers
static Tracker           tracker;       ///< Tracks P2P servers. Shared with publisher's node.

/// Default port number for the publisher's server and the multicast group
static constexpr int DEF_PORT = 38800;

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << log_getName() << " -h\n"
"    " << log_getName() << " -I [options]\n"
"    " << log_getName() << " [options]\n"
"Options:\n"
"  General:\n"
"    -c <configFile>   Pathname of configuration-file. Overrides previous\n"
"                      arguments; overridden by subsequent ones.\n"
"    -d <maxSegSize>   Maximum data-segment size in bytes. Default is " <<
                       RunPar::maxSegSize << ".\n"
"    -f <name>         Name of data-product feed. Default is \"" << RunPar::feedName << "\".\n"
"    -h                Print this help message on standard error, then exit.\n"
"    -I                Initialize only and then terminate. Default is to\n"
"                      initialize and then execute.\n"
"    -l <logLevel>     Logging level. <level> is \"FATAL\", \"ERROR\", \"WARN\",\n"
"                      \"NOTE\", \"INFO\", \"DEBUG\", or \"TRACE\". Comparison is case-\n"
"                      insensitive and takes effect immediately. Default is\n" <<
"                      \"" << RunPar::logLevel << "\".\n"
"    -r <pubRoot>      Pathname of publisher's root-directory. Default is\n"
"                      \"" << RunPar::pubRoot << "\".\n"
"    -t <trackerCap>   Maximum number of P2P servers to track. Default is " <<
                       RunPar::trackerCap << ".\n"
"  Publisher's Server:\n"
"    -P <pubAddr>      Socket address of publisher's server (not the P2P server).\n"
"                      Default is \"" << RunPar::pubSrvrAddr << "\".\n"
"    -Q <maxPending>   Maximum number of pending connections to publisher's\n"
"                      server (not the P2P server). Default is " << RunPar::pubSrvrQSize <<
                       ".\n"
"  Multicasting:\n"
"    -m <dstAddr>      Destination address of multicast group. Default is\n" <<
"                      \"" << RunPar::mcastDstAddr << "\".\n"
"    -s <srcAddr>      Internet address of multicast source/interface. Must not\n"
"                      be wildcard. Default is determined by operating system\n"
"                      based on destination address of multicast group.\n"
"  Peer-to-Peer:\n"
"    -b <interval>     Time between heartbeat packets in seconds. <0 => no\n"
"                      heartbeat. Default is " << RunPar::heartbeatInterval.count()*sysClockRatio <<
                       ".\n"
"    -e <evalTime>     Peer evaluation duration, in seconds, before replacing\n"
"                      poorest performer. Default is " << std::to_string(RunPar::peerEvalInterval)
                       << ".\n"
"    -n <maxPeers>     Maximum number of connected peers. Default is " << RunPar::maxNumPeers <<
                       ".\n"
"    -p <p2pAddr>      Socket address for local P2P server (not the publisher's\n"
"                      server). IP address must not be wildcard. Default IP\n"
"                      address is that of interface used for multicasting.\n"
"                      Default port number is 0.\n"
"    -q <maxPending>   Maximum number of pending connections to P2P server (not\n"
"                      the publisher's server). Default is " << RunPar::p2pSrvrQSize << ".\n"
"  Repository:\n"
"    -k <keepTime>     Number of seconds to keep data-products. Default is\n" <<
"                      " << std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                       RunPar::prodKeepTime).count()) << ".\n"
"    -o <maxOpenFiles> Maximum number of open repository files. Default is " <<
                       RunPar::maxOpenProds << ".\n"
"\n"
"SIGUSR2 rotates the logging level.\n";
}

/**
 * Sets publisher-specfic runtime parameters from a configuration-file.
 *
 * @param[in] pathname           Pathname of the configuration-file
 * @throw std::runtime_error     Parser failure
 */
static void setFromYaml(const String& pathname)
{
    auto node0 = YAML::LoadFile(pathname);

    try {
        Parser::tryDecode<decltype(RunPar::feedName)>(node0, "name", RunPar::feedName);
        Parser::tryDecode<decltype(RunPar::maxSegSize)>(node0, "maxSegSize", RunPar::maxSegSize);
        Parser::tryDecode<decltype(RunPar::trackerCap)>(node0, "trackerCap", RunPar::trackerCap);
        Parser::tryDecode<decltype(RunPar::pubRoot)>(node0, "rootDir", RunPar::pubRoot);

        auto node1 = node0["server"];
        if (node1) {
            auto node2 = node1["pubAddr"];
            if (node2)
                RunPar::pubSrvrAddr = SockAddr(node2.as<String>());
            Parser::tryDecode<decltype(RunPar::pubSrvrQSize)>(node1, "maxPending",
                    RunPar::pubSrvrQSize);
        }

        node1 = node0["multicast"];
        if (node1) {
            auto node2 = node1["dstAddr"];
            if (node2)
                RunPar::mcastDstAddr = SockAddr(node2.as<String>());
            node2 = node1["srcAddr"];
            if (node2)
                RunPar::mcastSrcAddr = InetAddr(node2.as<String>());
        }

        node1 = node0["repository"];
        if (node1) {
            int seconds;
            Parser::tryDecode<decltype(seconds)>(node1, "keepTime", seconds);
            RunPar::prodKeepTime = std::chrono::seconds(seconds);
        }
    } // YAML file loaded
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't parse YAML file \"" + pathname + "\""));
    }
}

/// Vets the publisher-specific runtime parameters.
static void vetRunPars()
{
    if (RunPar::mcastDstAddr.getInetAddr().getFamily() !=
            RunPar::mcastSrcAddr.getFamily())
        throw INVALID_ARGUMENT("Address of multicast group (" + RunPar::mcastDstAddr.to_string() +
                ") and address of multicast source (" + RunPar::mcastSrcAddr.to_string() +
                ") belong to different address families");

    if (RunPar::pubSrvrQSize <= 0)
        throw INVALID_ARGUMENT("Size of publisher's server-queue is not positive");

    if (RunPar::maxSegSize <= 0)
        throw INVALID_ARGUMENT("Maximum size of a data-segment is not positive");
    if (RunPar::maxSegSize > UdpSock::MAX_PAYLOAD)
        throw INVALID_ARGUMENT("Maximum size of a data-segment (" +
                std::to_string(RunPar::maxSegSize) + ") is greater than UDP maximum (" +
                std::to_string(UdpSock::MAX_PAYLOAD) + ")");
}

/// Initializes derived runtime parameters.
static void initDerived()
{
    subInfo.version = 1;
    subInfo.feedName = RunPar::feedName;
    subInfo.maxSegSize = RunPar::maxSegSize;
    subInfo.mcast.dstAddr = RunPar::mcastDstAddr;
    subInfo.mcast.srcAddr = RunPar::mcastSrcAddr;
    subInfo.tracker = Tracker(RunPar::trackerCap);
    subInfo.keepTime = RunPar::prodKeepTime;

    numSubThreads.setMax(RunPar::maxNumPeers);
}

/**
 * Sets the runtime parameters.
 *
 * @param[in] argc               Number of command-line arguments
 * @param[in] argv               Command-line arguments
 * @throw std::invalid_argument  Invalid option, option argument, or operand
 * @throw std::logic_error       Too many or too few operands
 */
static void setRunPars(
        const int    argc,
        char* const* argv)
{
    RunPar::init(argc, argv);

    // Set publisher-specific default runtime parameters
    RunPar::feedName     = String("<feedName>");
    RunPar::maxSegSize   = 20000;
    RunPar::mcastDstAddr = SockAddr("232.1.1.1", DEF_PORT);
    RunPar::mcastSrcAddr = InetAddr();
    RunPar::prodKeepTime = std::chrono::hours(1);
    RunPar::pubSrvrQSize = 256;
    RunPar::pubSrvrAddr  = SockAddr("0.0.0.0", DEF_PORT);

    try {
        opterr = 0;    // 0 => getopt() won't write to `stderr`
        int c;
        while ((c = ::getopt(argc, argv, RUNPAR_COMMON_OPTIONS_STRING "c:d:f:Ik:m:P:Q:r:s:")) != -1)
        {
            switch (c) {
                // Common options:
                RUNPAR_COMMON_OPTIONS_CASES(usage)

                // Publisher-specific options:
                case 'c': {
                    try {
                        RunPar::setFromYaml(optarg); // Sets common runtime parameters
                        setFromYaml(optarg);         // Sets program-specific runtime parameters
                    }
                    catch (const std::exception& ex) {
                        std::throw_with_nested(INVALID_ARGUMENT(
                                String("Couldn't initialize using configuration-file \"") + optarg +
                                "\""));
                    }
                    break;
                }
                case 'd': {
                    int maxSegSize;
                    if (::sscanf(optarg, "%d", &maxSegSize) != 1)
                        throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                                "\" option argument");
                    RunPar::maxSegSize = maxSegSize;
                    break;
                }
                case 'f': {
                    RunPar::feedName = String(optarg);
                    break;
                }
                case 'I': {
                    RunPar::initializeOnly = true;
                    break;
                }
                case 'k': {
                    int keepTime;
                    if (::sscanf(optarg, "%d", &keepTime) != 1)
                        throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                                "\" option argument");
                    RunPar::prodKeepTime = std::chrono::seconds(keepTime);
                    break;
                }
                case 'm': {
                    RunPar::mcastDstAddr = SockAddr(optarg);
                    break;
                }
                case 'P': {
                    RunPar::pubSrvrAddr = SockAddr(optarg, DEF_PORT);
                    break;
                }
                case 'Q': {
                    int size;
                    if (::sscanf(optarg, "%d", &size) != 1)
                        throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                                "\" option argument");
                    RunPar::pubSrvrQSize = size;
                    break;
                }
                case 'r': {
                    RunPar::pubRoot = String(optarg);
                    break;
                }
                case 's': {
                    RunPar::mcastSrcAddr = InetAddr(optarg);
                    break;
                }
            } // `switch` statement
        } // While getopt() loop

        if (optind != argc)
            throw LOGIC_ERROR("Too many operands specified");

        if (!RunPar::mcastSrcAddr)
            RunPar::mcastSrcAddr = UdpSock(RunPar::mcastDstAddr).getLclAddr().getInetAddr();

        if (!RunPar::p2pSrvrAddr)
            RunPar::p2pSrvrAddr = SockAddr(RunPar::mcastSrcAddr);

        RunPar::vet(); // Vets common runtime parameters
        vetRunPars(); // Vets publisher-specific runtime parameters
    }
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Error processing runtime parameters"));
    }
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
     * Bullshit! The system calls *should* be interrupted! Such calls should only be restarted for
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

/// Runs the publishing-node thread.
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
 * Runs the subscription-request thread.
 *
 * @param[in] xprt  Transport connected to the subscriber
 */
static void runSubRequest(Xprt xprt)
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
        LOG_INFO("Received tracker " + subTracker.to_string() + " from subscriber " +
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
        auto srvrSock = TcpSrvrSock(RunPar::pubSrvrAddr, RunPar::pubSrvrQSize);
        LOG_NOTE("Created publisher's server " + srvrSock.to_string());

        for (;;) {
            numSubThreads.waitToInc();
            auto xprt = Xprt(srvrSock.accept()); // RAII
            LOG_NOTE("Accepted connection from subscriber " + xprt.getRmtAddr().to_string());
            std::thread(&runSubRequest, xprt).detach();
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
 * @retval    1     Invocation error
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

        setRunPars(argc, argv);
        initDerived();

        tracker = Tracker(RunPar::trackerCap); // Create the tracker

        Thread serverThread;
        if (!RunPar::initializeOnly) {
            //LOG_DEBUG("Starting server thread");
            serverThread = Thread(&runServer);
        }

        pubNode = PubNode::create(tracker);

        if (!RunPar::initializeOnly) {
            if (::sem_init(&stopSem, 0, 0) == -1)
                    throw SYSTEM_ERROR("Couldn't initialize semaphore");

            setSigHandling(); // Catches termination signals

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
        }

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
        LOG_FATAL(ex);
        status = 2;
    }
    LOG_NOTE("Exiting with status %d", status);

    return status;
}
