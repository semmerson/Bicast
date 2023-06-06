/**
 * Program to subscribe to data-products via Hycast.
 *
 *        File: subscribe.cpp
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

#include "CommonTypes.h"
#include "Disposer.h"
#include "FileUtil.h"
#include "Node.h"
#include "Parser.h"
#include "PeerConn.h"
#include "Shield.h"
#include "Thread.h"
#include "ThreadException.h"

#include <semaphore.h>
#include <yaml-cpp/yaml.h>

/// Default maximum number of active peers
static constexpr int DEF_MAX_PEERS    = 8;
/// Default multicast group Internet address
static constexpr char DEF_MCAST_SPEC[] = "232.1.1.1";
/// Default port for multicast group
static constexpr int DEF_PORT         = 38800;
/// Default tracker size
static constexpr int DEF_TRACKER_SIZE = 1000;
/// Default timeout in ms for connecting to remote P2P server
static constexpr int DEF_TIMEOUT      = 15000;
/// Default peer evaluation duration
static constexpr int DEF_EVAL_DURATION = 300;

using namespace hycast;

using String = std::string;

/// Runtime parameters of this program
struct RunPar {
    LogLevel  logLevel;   ///< Logging level
    SockAddr  pubAddr;    ///< Address of publisher
    InetAddr  mcastIface; ///< Address of interface for multicast reception. May be wildcard.
    /// Runtime parameters for a subscriber's P2P manager
    struct P2pArgs {      ///< P2P runtime parameters
        struct SrvrArgs {
            SockAddr addr;        ///< P2P server's address. Must not be wildcard.
            int      maxPendConn; ///< Maximum number of pending P2P connections
            /**
             * Constructs.
             * @param[in] addr         Socket address for the local P2P server
             * @param[in] maxPendConn  Maximum number of pending connections
             */
            SrvrArgs(
                    const SockAddr& addr,
                    const int       maxPendConn)
                : addr(addr)
                , maxPendConn(maxPendConn)
            {}
        }   srvr;        ///< P2P server runtime parameters
        int timeout;     ///< Timeout in ms for connecting to remote P2P server
        int trackerSize; ///< Capacity of tracker object
        int maxPeers;    ///< Maximum number of peers to have
        int evalTime;    ///< Time interval for evaluating peer performance in seconds
        /**
         * Constructs.
         * @param[in] addr         Socket address for the local P2P server
         * @param[in] acceptQSize  Size of the `listen()` queue
         * @param[in] timeout      Timeout in ms for connecting to remote P2P server
         * @param[in] trackerSize  Capacity of the tracker object
         * @param[in] maxPeers     Maximum number of neighboring peers
         * @param[in] evalTime     Duration over which to evaluate the peers
         */
        P2pArgs(const SockAddr& addr,
                const int       acceptQSize,
                const int       timeout,
                const int       trackerSize,
                const int       maxPeers,
                const int       evalTime)
            : srvr(addr, DEF_MAX_PEERS)
            , timeout(timeout)
            , trackerSize(trackerSize)
            , maxPeers(maxPeers)
            , evalTime(evalTime)
        {}
    }         p2p; ///< P2P runtime parameters
    /// Runtime parameters for the repository
    struct RepoArgs { ///< Repository runtime parameters
        String   rootDir;      ///< Pathname of the repository's root directory
        int      maxOpenFiles; ///< Maximum number of open files
        /**
         * Constructs.
         * @param[in] rootDir       Pathname of the root of the repository
         * @param[in] maxOpenFiles  Maximum number of open file descriptors
         */
        RepoArgs(
                const String&  rootDir,
                const unsigned maxOpenFiles)
            : rootDir(rootDir)
            , maxOpenFiles(maxOpenFiles)
        {}
    }         repo; ///< Repository runtime parameters
    String dispositionFile; ///< Pathname of YAML file indicating how products should be processed

    /**
     * Default constructs.
     */
    RunPar()
        : logLevel(LogLevel::NOTE)
        , pubAddr()
        , mcastIface("0.0.0.0") // Might get changed to match family of multicast group
        , p2p(SockAddr(), DEF_MAX_PEERS, DEF_TIMEOUT, DEF_TRACKER_SIZE, DEF_MAX_PEERS,
                DEF_EVAL_DURATION)
        , repo("repo", ::sysconf(_SC_OPEN_MAX)/2)
        , dispositionFile()
    {}
};

static sem_t               stopSem;    ///< Semaphore for async-signal-safe stopping
static ThreadEx            threadEx;   ///< Exception thrown by a thread
static RunPar              runPar;     ///< Runtime parameters:
static const RunPar        defRunPar;  ///< Default runtime parameters

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << log_getName() << " -h\n"
"    " << log_getName() << " [-c <configFile>] [-e <evalTime>] [-i <p2pAddr>] [-l <level>]\n"
"        [-m <maxPeers>] [-o <maxOpenFiles>] [-q <maxPending>] [-r <repoRoot>]\n"
"        [-t <trackerSize>] <pubAddr>[:<port>]\n"
"where:\n"
"  General:\n"
"    -c <configFile>    Pathname of configuration-file. Overrides previous\n"
"                       arguments; overridden by subsequent ones.\n"
"    -h                 Print this help message on standard error, then exit.\n"
"    -l <level>         Logging level. <level> is \"FATAL\", \"ERROR\", \"WARN\",\n"
"                       \"NOTE\", \"INFO\", \"DEBUG\", or \"TRACE\". Comparison is case-\n"
"                       insensitive and takes effect immediately. Default is\n"
"                       \"" << defRunPar.logLevel << "\".\n"
"    <pubAddr>[:<port>] Socket address of publisher's server. Default port number\n"
"                       is " << DEF_PORT << ".\n"
"  Peer-to-Peer:\n"
"    -e <evalTime>      Peer evaluation duration, in seconds, before replacing\n"
"                       poorest performer. Default is " << defRunPar.p2p.evalTime << ".\n"
"    -p <p2pAddr>       Internet address for local P2P server Must not be\n"
"                       wildcard. Default is IP address of interface used to\n"
"                       connect to publisher.\n"
"    -n <maxPeers>      Maximum number of connected peers. Default is " << defRunPar.p2p.maxPeers <<
                        ".\n"
"    -T <timeout>       Timeout, in ms, for connecting to remote P2P server.\n"
"                       Default is " << defRunPar.p2p.timeout << ".\n"
"    -q <maxPending>    Maximum number of pending connections to P2P server.\n"
"                       Default is " << defRunPar.p2p.srvr.maxPendConn << ".\n"
"    -t <trackerSize>   Maximum number of P2P servers to track. Default is " <<
                        defRunPar.p2p.trackerSize << ".\n"
"  Repository:\n"
"    -o <maxOpenFiles>  Maximum number of open repository files. Default is " <<
                        defRunPar.repo.maxOpenFiles << ".\n"
"    -r <repoRoot>      Pathname of root of publisher's repository. Default is\n"
"                       \"" << defRunPar.repo.rootDir << "\".\n"
"  Product Disposition:\n"
"    -d <configFile>    Pathname of configuration-file specifying disposition of\n"
"                       products. No local processing if empty string (default).\n"
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
                auto node3 = node2["InetAddr"];
                if (node3)
                    runPar.p2p.srvr.addr = SockAddr(node3.as<String>(), 0);
                Parser::tryDecode<decltype(runPar.p2p.srvr.maxPendConn)>(node2, "QueueSize",
                        runPar.p2p.srvr.maxPendConn);
            }

            Parser::tryDecode<decltype(runPar.p2p.timeout)>(node1, "Timeout", runPar.p2p.timeout);
            Parser::tryDecode<decltype(runPar.p2p.maxPeers)>(node1, "MaxPeers",
                    runPar.p2p.maxPeers);
            Parser::tryDecode<decltype(runPar.p2p.trackerSize)>(node1, "TrackerSize",
                    runPar.p2p.trackerSize);
            Parser::tryDecode<decltype(runPar.p2p.evalTime)>(node1, "EvalTime",
                    runPar.p2p.evalTime);
        }

        node1 = node0["Repository"];
        if (node1) {
            Parser::tryDecode<decltype(runPar.repo.rootDir)>(node1, "Pathname",
                    runPar.repo.rootDir);
            Parser::tryDecode<decltype(runPar.repo.maxOpenFiles)>(node1, "MaxOpenFiles",
                    runPar.repo.maxOpenFiles);
        }

        node1 = node0["Disposition"];
        if (node1) {
            Parser::tryDecode<decltype(runPar.dispositionFile)>(node1, "Disposition",
                    runPar.dispositionFile);
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
    if (runPar.p2p.srvr.maxPendConn <= 0)
        throw INVALID_ARGUMENT("Size of local P2P server's listen() queue is not positive");
    if (runPar.p2p.timeout < -1)
        throw INVALID_ARGUMENT("P2p connection timeout is less than -1");
    if (runPar.p2p.maxPeers <= 0)
        throw INVALID_ARGUMENT("Maximum number of peers is not positive");
    if (runPar.p2p.trackerSize <= 0)
        throw INVALID_ARGUMENT("Tracker size is not positive");
    if (runPar.p2p.evalTime <= 0)
        throw INVALID_ARGUMENT("Peer performance evaluation-duration is not positive");

    if (runPar.repo.rootDir.empty())
        throw INVALID_ARGUMENT("Name of repository's root-directory is the empty string");
    if (runPar.repo.maxOpenFiles <= 0)
        throw INVALID_ARGUMENT("Maximum number of open repository files is not positive");
    if (runPar.repo.maxOpenFiles > ::sysconf(_SC_OPEN_MAX))
        throw INVALID_ARGUMENT("Maximum number of open repository files is "
                "greater than system maximum, " + std::to_string(sysconf(_SC_OPEN_MAX)));

    if (runPar.dispositionFile.size() && !FileUtil::exists(runPar.dispositionFile))
        throw INVALID_ARGUMENT("Configuration-file for disposition of products, \"" +
                runPar.dispositionFile + "\", doesn't exist");
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
    runPar = defRunPar;

    opterr = 0;    // 0 => getopt() won't write to `stderr`
    int c;
    while ((c = ::getopt(argc, argv, ":c:d:e:hl:m:o:p:q:r:T:")) != -1) {
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
            runPar.dispositionFile = String(optarg);
            break;
        }
        case 'e': {
            int evalTime;
            if (::sscanf(optarg, "%d", &evalTime) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (evalTime <= 0)
                throw INVALID_ARGUMENT("Peer performance duration is not positive");
            runPar.p2p.evalTime = evalTime;
            break;
        }
        case 'l': {
            log_setLevel(optarg);
            runPar.logLevel = log_getLevel();
            break;
        }
        case 'n': {
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
        case 'q': {
            if (::sscanf(optarg, "%d", &runPar.p2p.srvr.maxPendConn) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (runPar.p2p.srvr.maxPendConn <= 0)
                throw INVALID_ARGUMENT("Size of P2P server's listen() queue is not positive");
            break;
        }
        case 'r': {
            runPar.repo.rootDir = String(optarg);
            if (runPar.repo.rootDir.empty())
                throw INVALID_ARGUMENT("Name of repository's root-directory is the empty string");
            break;
        }
        case 'T': {
            if (::sscanf(optarg, "%d", &runPar.p2p.timeout) != 1)
                throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                        "\" option argument");
            if (runPar.p2p.timeout < -1)
                throw INVALID_ARGUMENT("P2P server connection-time is less than -1");
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
            throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(optopt) + "\" option");
        }
        default : { // c == '?'
            throw INVALID_ARGUMENT(String("Unknown \"-") + static_cast<char>(optopt) + "\" option");
        }
        } // `switch` statement
    } // While getopt() loop

    if (argv[optind] == nullptr)
        throw INVALID_ARGUMENT("Publisher's socket address wasn't specified");
    runPar.pubAddr = SockAddr(argv[optind++]);

    if (optind != argc)
        throw INVALID_ARGUMENT("Excess arguments were specified");

    /*
     * If the Internet address for the P2P server wasn't specified, then it's determined from the
     * interface used to connect to the publisher's server.
     */
    if (!runPar.p2p.srvr.addr)
        runPar.p2p.srvr.addr = SockAddr(UdpSock(runPar.pubAddr).getLclAddr().getInetAddr(), 0);

    vetRunPars();
}

/// Sets the exception thrown on an internal thread.
static void setException(const std::exception& ex)
{
    threadEx.set(ex);
    ::sem_post(&stopSem);
}

/**
 * Performs local processing of complete data-products.
 */
static void runProdProc(
        SubNodePtr subNode,
        Disposer   disposer) {
    try {
        for (;;) {
            auto prodEntry = subNode->getNextProd();
            Shield shield{}; // Protects product disposition from cancellation
            LOG_DEBUG("Disposing of " + prodEntry.to_string());
            disposer.dispose(prodEntry.getProdInfo(), prodEntry.getData());
        }
    }
    catch (const std::exception& ex) {
        setException(ex);
    }
}

static std::thread startProdProc(
        SubNodePtr subNode,
        Disposer   disposer) {
    try {
        return std::thread(&runProdProc, subNode, disposer);
    }
    catch (const std::exception& ex) {
        std::throw_with_nested(RUNTIME_ERROR("Couldn't create thread to process products"));
    }
}

/**
 * Stops product processing. Shouldn't block for long.
 */
static void stopProdProc(std::thread& procThread) {
    if (procThread.joinable()) {
        ::pthread_cancel(procThread.native_handle());
        procThread.join();
    }
}

/// Creates the subscriber node.
static SubNodePtr createSubNode()
{
    // Keep consonant with `publish.cpp:servSubscriber()`

    LOG_DEBUG("Creating peer-connection server");
    auto peerConnSrvr = PeerConnSrvr::create(runPar.p2p.srvr.addr, runPar.p2p.srvr.maxPendConn);

    LOG_NOTE("Connecting to publisher " + runPar.pubAddr.to_string());
    Xprt    xprt{TcpClntSock(runPar.pubAddr)}; // RAII object

    P2pSrvrInfo p2pSrvrInfo{peerConnSrvr->getSrvrAddr(), (runPar.p2p.maxPeers+1)/2};
    LOG_DEBUG("Sending information on P2P server " + peerConnSrvr->getSrvrAddr().to_string() +
            " to publisher " + runPar.pubAddr.to_string());

    bool lostConnection = true;
    if (!p2pSrvrInfo.write(xprt)) {
        throw RUNTIME_ERROR("Couldn't send P2P-server info to publisher " +
                runPar.pubAddr.to_string());
    }
    else {
        P2pSrvrInfo pubP2pSrvrInfo;
        if (!pubP2pSrvrInfo.read(xprt)) {
            throw RUNTIME_ERROR("Couldn't receive P2P-server info from publisher " +
                    runPar.pubAddr.to_string());
        }
        else {
            LOG_INFO("Received information on publisher's P2P server: " +
                    pubP2pSrvrInfo.to_string());

            SubInfo subInfo;
            if (!subInfo.read(xprt)) {
                throw RUNTIME_ERROR("Couldn't receive subscription info from publisher " +
                        runPar.pubAddr.to_string());
            }
            else {
                LOG_INFO("Received subscription information from publisher " +
                        runPar.pubAddr.to_string() + ": #peers=" +
                        std::to_string(subInfo.tracker.size()));

                subInfo.tracker.insert(pubP2pSrvrInfo);
                DataSeg::setMaxSegSize(subInfo.maxSegSize);

                // Address family of receiving interface should match that of multicast group
                if (runPar.mcastIface.isAny()) {
                    // The following doesn't work if the outgoing multicast interface is localhost
                    //runPar.mcastIface = subInfo.mcast.dstAddr.getInetAddr().getWildcard();
                    // The following works in that context
                    runPar.mcastIface =
                            UdpSock(SockAddr(subInfo.mcast.srcAddr, 0)).getLclAddr().getInetAddr();
                    //LOG_DEBUG("Set interface for multicast reception to " +
                            //runPar.mcastIface.to_string());
                }

                //LOG_DEBUG("Creating subscribing node");
                return SubNode::create(subInfo, runPar.mcastIface, peerConnSrvr, runPar.p2p.timeout,
                        runPar.p2p.maxPeers, runPar.p2p.evalTime, runPar.repo.rootDir,
                        runPar.repo.maxOpenFiles);
            } // Received subscription information from publisher
        } // Received information on publisher's P2P-server
    } // Sent information on subscriber's P2P-server to publisher
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
 * Sets the signal handler.
 */
static void setSigHand()
{
    log_setLevelSignal(SIGUSR2);

    struct sigaction sigact;
    (void) sigemptyset(&sigact.sa_mask);
    /*
     * System calls interrupted by a termination signal are restarted because termination is more
     * cleanly handled by this application.
     */
    sigact.sa_flags = SA_RESTART;
    sigact.sa_handler = &sigHandler;
    (void)sigaction(SIGINT, &sigact, NULL);
    (void)sigaction(SIGTERM, &sigact, NULL);
}

/// Executes the subscribing node.
static void runNode(SubNodePtr subNode)
{
    try {
        subNode->run();
    }
    catch (const std::exception& ex) {
        setException(ex);
    }
}

void stopSubNode(
        SubNodePtr   subNode,
        std::thread& nodeThread)
{
    if (nodeThread.joinable()) {
        subNode->halt(); // Idempotent
        nodeThread.join();
    }
}

/**
 * Subscribes to data-products.
 *
 * @param[in] argc  Number of command-line arguments
 * @param[in] argv  Command-line arguments
 * @retval    0     Success
 * @retval    1     Command-line error
 * @retval    2     Runtime error
 */
int main(
        const int    argc,
        char* const* argv)
{
    int status = 2;

    std::set_terminate(&terminate); // NB: Hycast version

    try {
        log_setName(::basename(argv[0]));
        LOG_NOTE("Starting up: " + getCmdLine(argc, argv));

        getCmdPars(argc, argv);

        if (::sem_init(&stopSem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");

        try {
            Disposer disposer{};
            if (runPar.dispositionFile.size())
                disposer = Disposer::createFromYaml(runPar.dispositionFile);

            auto subNode = createSubNode();
            auto procThread = startProdProc(subNode, disposer);

            try {
                auto nodeThread = std::thread(&runNode, subNode);

                try {
                    setSigHand(); // Catches termination signals

                    ::sem_wait(&stopSem); // Returns if failure on a thread or termination signal

                    threadEx.throwIfSet(); // Throws if failure on a thread

                    stopSubNode(subNode, std::ref(nodeThread));
                    stopProdProc(std::ref(procThread));
                    ::sem_destroy(&stopSem);

                    status = 0;
                }
                catch (const std::exception& ex) {
                    stopSubNode(subNode, std::ref(nodeThread));
                    throw;
                }
            }
            catch (const std::exception& ex) {
                stopProdProc(std::ref(procThread));
                throw;
            }
        }
        catch (const std::exception& ex) {
            ::sem_destroy(&stopSem);
            throw;
        }
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
