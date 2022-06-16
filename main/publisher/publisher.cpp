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

#include "Node.h"
#include "SubInfo.h"
#include "ThreadException.h"

#include <inttypes.h>
#include <semaphore.h>
#include <thread>
#include <yaml-cpp/yaml.h>

using namespace hycast;

using String = std::string;

static constexpr int DEF_LISTEN_SIZE = 8;

/// Command-line/configuration-file parameters of this program
struct RunPar {
    String    feedName;                   ///< Name of data-product stream
    LogLevel  logLevel;                   ///< Logging level
    int32_t   maxSegSize;                 ///< Maximum size of a data-segment in bytes
    struct Srvr {
        SockAddr      addr;               ///< Socket address
        int           listenSize;         ///< Size of `::listen()` queue
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
        , srvr(SockAddr("0.0.0.0:38800"), DEF_LISTEN_SIZE)
        , mcast(SockAddr("232.1.1.1:38800"), InetAddr())
        , p2p(SockAddr(), 8, 8, 100, 60)
        , repo("repo", maxSegSize, ::sysconf(_SC_OPEN_MAX)/2, 3600)
    {
        mcast.srcAddr = UdpSock(mcast.dstAddr).getLclAddr().getInetAddr();
        p2p.srvr.addr = SockAddr(mcast.srcAddr, 0);
    }
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
        , max(DEF_LISTEN_SIZE)
        , count(0)
    {}
    void setMax(const int max) {
        this->max = max;
    }
    void waitToInc() {
        Lock lock{mutex};
        cond.wait(lock, [&]{return count < max;});
        ++count;
    }
    void operator--() {
        Guard guard{mutex};
        --count;
        cond.notify_all();
    }
}                        numSubThreads; ///< Subscriber thread count
static sem_t             sem;           ///< Semaphore for async-signal-safe state changes
static std::atomic<bool> stop;          ///< Should the program stop?
static ThreadEx          threadEx;      ///< Exception thrown by a thread
static RunPar            runPar;        ///< Runtime parameters:
static const RunPar      defRunPar;     ///< Default runtime parameters
static PubNode::Pimpl    pubNode;       ///< Data-product publishing node
static SubInfo           subInfo;       ///< Subscription information passed to subscribers

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << log_getName() << " -h\n"
"    " << log_getName() << " [-c <configFile>] [-e <evalTime>] [-f <name>] [-k <keepTime>]\n"
"        [-l <level>] [-M <mcastAddr>] [-m <maxPeers>] [-o <maxOpenFiles>]\n"
"        [-P <pubAddr>] [-p <p2pAddr>] [-Q <size>] [-q <size>]\n"
"        [-r <repoRoot>] [-S <srcAddr>] [-s <maxSegSize>] [-t <trackerSize>]\n"
"where:\n"
"    -h                Print this help message on standard error, then exit.\n"
"\n"
"    -c <configFile>   Pathname of configuration-file. Overrides previous\n"
"                      arguments; overridden by subsequent ones.\n"
"    -e <evalTime>     Peer evaluation duration, in seconds, before replacing\n"
"                      poorest performer. Default is " << defRunPar.p2p.evalTime << ".\n"
"    -f <name>         Name of data-product feed. Default is \"" << defRunPar.feedName << "\".\n"
"    -k <keepTime>     How long to keep data-products in seconds. Default is\n" <<
"                      " << defRunPar.repo.keepTime << " s.\n"
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
"    -P <pubAddr>      Socket address of the publisher (not its P2P server).\n"
"                      Default is \"" << defRunPar.srvr.addr << "\".\n"
"    -p <p2pAddr>      Internet address of local P2P server (not the publisher).\n"
"                      Default is \"" << defRunPar.p2p.srvr.addr.getInetAddr() << "\".\n"
"    -Q <size>         Maximum number of pending connections to publisher's\n"
"                      server (not the P2P server). Default is " << defRunPar.srvr.listenSize <<
                       ".\n"
"    -q <size>         Maximum number of pending connections to P2P server (not\n"
"                      the publisher's server). Default is " << defRunPar.p2p.srvr.acceptQSize <<
                       ".\n"
"    -r <repoRoot>     Pathname of root of publisher's repository. Default is\n"
"                      \"" << defRunPar.repo.rootDir << "\".\n"
"    -S <srcAddr>      Internet address of multicast source (i.e., multicast\n"
"                      interface). Default is \"" << defRunPar.mcast.srcAddr << "\".\n"
"    -s <maxSegSize>   Maximum size of a data-segment in bytes. Default is " <<
                       defRunPar.maxSegSize << ".\n"
"    -t <trackerSize>  Maximum size of the list of remote P2P servers. Default is\n" <<
"                      " << defRunPar.p2p.trackerSize << ".\n"
"\n"
"SIGUSR2 rotates the logging level.\n";
}

/**
 * Tries to decode a scalar (i.e., primitive) value in a YAML map.
 *
 * @tparam     T                 Type of scalar value
 * @param[in]  parent            Map containing the scalar
 * @param[in]  key               Name of the scalar
 * @param[out] value             Scalar value
 * @throw std::invalid_argument  Parent node isn't a map
 * @throw std::invalid_argument  Subnode with given name isn't a scalar
 */
template<class T>
static void tryDecode(YAML::Node&   parent,
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
        tryDecode<decltype(runPar.feedName)>(node0, "Name", runPar.feedName);
        auto node1 = node0["LogLevel"];
        if (node1)
            log_setLevel(node1.as<String>());
        tryDecode<decltype(runPar.maxSegSize)>(node0, "MaxSegSize", runPar.maxSegSize);

        node1 = node0["Server"];
        if (node1) {
            auto node2 = node1["SockAddr"];
            if (node2)
                runPar.srvr.addr = SockAddr(node2.as<String>());
            tryDecode<decltype(runPar.srvr.listenSize)>(node1, "QueueSize",
                    runPar.srvr.listenSize);
        }

        node1 = node0["Multicast"];
        if (node1) {
            auto node2 = node1["DstAddr"];
            if (node2)
                runPar.mcast.dstAddr = SockAddr(node2.as<String>());
            node2 = node1["SrcAddr"];
            if (node2)
                runPar.mcast.srcAddr = InetAddr(node2.as<String>());
        }

        node1 = node0["Peer2Peer"];
        if (node1) {
            auto node2 = node1["Server"];
            if (node2) {
                auto node3 = node2["IfaceAddr"];
                if (node3)
                    runPar.p2p.srvr.addr = SockAddr(node3.as<String>(), 0);
                tryDecode<decltype(runPar.p2p.srvr.acceptQSize)>(node2, "QueueSize",
                        runPar.p2p.srvr.acceptQSize);
            }

            tryDecode<decltype(runPar.p2p.maxPeers)>(node1, "MaxPeers", runPar.p2p.maxPeers);
            tryDecode<decltype(runPar.p2p.trackerSize)>(node1, "TrackerSize",
                    runPar.p2p.trackerSize);
            tryDecode<decltype(runPar.p2p.evalTime)>(node1, "EvalTime", runPar.p2p.evalTime);
        }

        node1 = node0["Repository"];
        if (node1) {
            tryDecode<decltype(runPar.repo.rootDir)>(node1, "Pathname", runPar.repo.rootDir);
            tryDecode<decltype(runPar.repo.maxOpenFiles)>(node1, "MaxOpenFiles",
                    runPar.repo.maxOpenFiles);
            tryDecode<decltype(runPar.repo.keepTime)>(node1, "KeepTime", runPar.repo.keepTime);
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
        throw INVALID_ARGUMENT("Size of publisher's server-queue is not positive");

    if (runPar.p2p.srvr.acceptQSize <= 0)
        throw INVALID_ARGUMENT("Size of P2P server-queue is not positive");

    if (runPar.repo.rootDir.empty())
        throw INVALID_ARGUMENT("Name of repository's root-directory is the empty string");

    if (runPar.maxSegSize <= 0)
        throw INVALID_ARGUMENT("Maximum size of a data-segment is not positive");
    if (runPar.maxSegSize > UdpSock::MAX_PAYLOAD)
        throw INVALID_ARGUMENT("Maximum size of a data-segment (" +
                std::to_string(runPar.maxSegSize) + ") is greater than UDP maximum (" +
                std::to_string(UdpSock::MAX_PAYLOAD) + ")");
}

/// Initializes runtime parameters that aren't set from the command-line/configuration-file
static void init()
{
    DataSeg::setMaxSegSize(runPar.maxSegSize);

    if (::sem_init(&sem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");

    subInfo.version = 1;
    subInfo.feedName = runPar.feedName;
    subInfo.maxSegSize = runPar.maxSegSize;
    subInfo.mcast.dstAddr = runPar.mcast.dstAddr;
    subInfo.mcast.srcAddr = runPar.mcast.srcAddr;
    subInfo.tracker = Tracker(runPar.p2p.trackerSize);
    subInfo.keepTime = runPar.repo.keepTime;

    numSubThreads.setMax(runPar.srvr.listenSize);
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
    log_setName(::basename(argv[0]));
    runPar = defRunPar;

    opterr = 0;    // 0 => getopt() won't write to `stderr`
    int c;
    while ((c = ::getopt(argc, argv, ":c:e:f:hk:l:M:m:o:P:p:Q:q:r:S:s:t:")) != -1) {
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
        case 'L': {
            int trackerSize;
            if (::sscanf(optarg, "%d", &trackerSize) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
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
            runPar.repo.rootDir = String(optarg);
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
}

/**
 * Signals the program to halt.
 */
static void halt()
{
    ::sem_post(&sem);
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
    sigact.sa_flags = SA_RESTART; // Restart interrupted system calls
    sigact.sa_handler = &sigHandler;
    (void)sigaction(SIGINT, &sigact, NULL);
    (void)sigaction(SIGTERM, &sigact, NULL);
}

/// Sets the exception thrown on an internal thread.
static void setException(const std::exception& ex)
{
    threadEx.set(ex);
    halt();
}

/// Runs the publishing node.
static void runNode()
{
    try {
        pubNode->run();
    }
    catch (const std::exception& ex) {
        setException(ex);
    }
}

/**
 * Handles a subscription request.
 *
 * @param[in] xprt  Transport connected to the subscriber
 */
static void handleSubscriber(Xprt xprt)
{
    try {
        // Keep consonant with `Subscriber::createSubNode()`
        SockAddr p2pSrvrAddr;
        if (subInfo.write(xprt) && p2pSrvrAddr.read(xprt))
            subInfo.tracker.insert(p2pSrvrAddr);
        --numSubThreads;
    }
    catch (const std::exception& ex) {
        LOG_ERROR("Couldn't serve subscriber %s: %s", xprt.getRmtAddr().to_string().data(),
                ex.what());
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
        auto srvrSock = TcpSrvrSock(runPar.srvr.addr, runPar.srvr.listenSize);

        subInfo.tracker.insert(pubNode->getP2pSrvrAddr());

        for (;;) {
            numSubThreads.waitToInc();
            auto xprt = Xprt(srvrSock.accept()); // RAII
            std::thread(&handleSubscriber, xprt).detach();
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
        getCmdPars(argc, argv);
        init();

        pubNode = PubNode::create(runPar.maxSegSize, runPar.mcast, runPar.p2p, runPar.repo);
        setSigHandling(); // Catches termination signals

        auto nodeThread = Thread(&runNode);
        auto serverThread = Thread(&runServer);

        ::sem_wait(&sem); // Returns if failure on a thread or termination signal
        ::sem_destroy(&sem);

        ::pthread_cancel(serverThread.native_handle());
        pubNode->halt(); // Idempotent

        serverThread.join();
        nodeThread.join();

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
