/**
 * @file subscribe.cpp
 * Program to subscribe to data-products via Bicast.
 *
 * @section Legal
 * @copyright 2022 University Corporation for Atmospheric Research
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
 *
 * @author Steven R. Emmerson <emmerson@ucar.edu>
 * @date 2023-07-20
 */

#include "config.h"

#include "CommonTypes.h"
#include "Disposer.h"
#include "FileUtil.h"
#include "Node.h"
#include "P2pSrvrInfo.h"
#include "PeerConn.h"
#include "Parser.h"
#include "RunPar.h"
#include "Shield.h"
#include "Thread.h"
#include "ThreadException.h"

#include <atomic>
#include <semaphore.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

/// Default port for multicast group
static constexpr int DEF_PORT         = 38800;

using namespace bicast;

using String = std::string; ///< String type

static ThreadEx            threadEx;    ///< Exception thrown by a thread
static Tracker             subTracker;  ///< Subscriber's tracker of P2P servers
static std::atomic_bool    done;        ///< Is this program done?

static void usage()
{
    std::cerr <<
"Usage:\n"
"    " << log_getName() << " -h\n"
"    " << log_getName() << " [options] <pubAddr>[:<port>]\n"
"Options:\n"
"  General:\n"
"    -c <ConfigFile>     Pathname of configuration-file. Overrides previous\n"
"                        arguments; overridden by subsequent ones.\n"
"    -h                  Print this help message on standard error, then exit.\n"
"    -i <retryInterval>  Seconds to wait after a receiving session terminates\n"
"                        due to a non-fatal error before retrying. Default is " <<
                         std::to_string(RunPar::retryInterval) << ".\n"
"    -l <logLevel>       Logging level. <level> is \"FATAL\", \"ERROR\", \"WARN\",\n"
"                        \"NOTE\", \"INFO\", \"DEBUG\", or \"TRACE\". Comparison is case-\n"
"                        insensitive and takes effect immediately. Default is\n"
"                        \"" << RunPar::logLevel << "\".\n"
"    -r <subRoot>        Pathname of subscriber's root-directory. Default is \"" <<
                         RunPar::subRoot << "\".\n"
"  Peer-to-Peer:\n"
"    -e <evalTime>       Peer evaluation duration, in seconds, before replacing\n"
"                        poorest performer. Default is " << RunPar::peerEvalInterval << ".\n"
"    -p <p2pAddr>        Internet address for local P2P server Must not be\n"
"                        wildcard. Default is IP address of interface used to\n"
"                        connect to publisher.\n"
"    -n <maxPeers>       Maximum number of connected peers. Default is " << RunPar::maxNumPeers
                         << ".\n"
"    -q <maxPending>     Maximum number of pending connections to P2P server.\n"
"                        Default is " << RunPar::p2pSrvrQSize << ".\n"
"    -t <trackerSize>    Maximum number of P2P servers to track. Default is " <<
                         RunPar::trackerCap << ".\n"
"  Repository:\n"
"    -o <maxOpenFiles>   Maximum number of open repository files. Default is " <<
                         RunPar::maxOpenProds << ".\n"
"  Product Disposition:\n"
"    -d <disposeConfig>  Pathname of configuration-file specifying disposition of\n"
"                        products. The default is no local processing.\n"
"Operands:\n"
"    <pubAddr>[:<port>]  Socket address of publisher's server. Default port\n"
"                        number is " << DEF_PORT << ".\n"
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
        auto node1 = node0["logLevel"];
        if (node1)
            log_setLevel(node1.as<String>());

        Parser::tryDecode<decltype(RunPar::subRoot)>(node0, "rootDir", RunPar::subRoot);

        node1 = node0["pubAddr"];
        if (node1)
            RunPar::pubSrvrAddr = SockAddr(node1.as<String>(), DEF_PORT);

        int seconds;
        Parser::tryDecode<decltype(seconds)>(node0, "retryInterval", seconds);
        RunPar::retryInterval = std::chrono::seconds(seconds);

        node1 = node0["peer2Peer"];
        if (node1) {
            auto node2 = node1["server"];
            if (node2) {
                auto node3 = node2["p2pAddr"];
                if (node3)
                    RunPar::p2pSrvrAddr = SockAddr(node3.as<String>());
                Parser::tryDecode<decltype(RunPar::p2pSrvrQSize)>(node2, "maxPending",
                        RunPar::p2pSrvrQSize);
            }

            Parser::tryDecode<decltype(RunPar::maxNumPeers)>(node1, "maxPeers",
                    RunPar::maxNumPeers);
            Parser::tryDecode<decltype(RunPar::trackerCap)>(node1, "trackerSize",
                    RunPar::trackerCap);
            int seconds;
            Parser::tryDecode<decltype(seconds)>(node1, "evalTime", seconds);
            RunPar::peerEvalInterval = std::chrono::seconds(seconds);
        }

        node1 = node0["repository"];
        if (node1) {
            Parser::tryDecode<decltype(RunPar::maxOpenProds)>(node1, "maxOpenFiles",
                    RunPar::maxOpenProds);
        }

        Parser::tryDecode<decltype(RunPar::disposeConfig)>(node0, "disposeConfig",
                RunPar::disposeConfig);
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
    if (!RunPar::pubSrvrAddr)
        throw INVALID_ARGUMENT("Publisher's socket address wasn't specified");

    if (!RunPar::p2pSrvrAddr)
        throw INVALID_ARGUMENT("IP address for local P2P server wasn't specified");
    if (RunPar::p2pSrvrQSize <= 0)
        throw INVALID_ARGUMENT("Size of local P2P server's listen() queue is not positive");
    if (RunPar::maxNumPeers <= 0)
        throw INVALID_ARGUMENT("Maximum number of peers is not positive");
    if (RunPar::trackerCap <= 0)
        throw INVALID_ARGUMENT("Tracker size is not positive");
    if (RunPar::peerEvalInterval.count() <= 0)
        throw INVALID_ARGUMENT("Peer performance evaluation-duration is not positive");

    if (RunPar::subRoot.empty())
        throw INVALID_ARGUMENT("Name of subscriber's root-directory is the empty string");
    if (RunPar::maxOpenProds <= 0)
        throw INVALID_ARGUMENT("Maximum number of open repository files is not positive");
    if (RunPar::maxOpenProds > ::sysconf(_SC_OPEN_MAX))
        throw INVALID_ARGUMENT("Maximum number of open repository files is "
                "greater than system maximum, " + std::to_string(sysconf(_SC_OPEN_MAX)));

    if (RunPar::disposeConfig.size() && !FileUtil::exists(RunPar::disposeConfig))
        throw INVALID_ARGUMENT("Configuration-file for disposition of products, \"" +
                RunPar::disposeConfig + "\", doesn't exist");
}

/**
 * Sets runtime parameters.
 *
 * @throw std::invalid_argument  Invalid option, option argument, or variable value
 */
static void setRunPars(
        const int    argc, ///< Number of command-line arguments
        char* const* argv) ///< Command-line arguments
{
    RunPar::init(argc, argv);

    // Set subscriber-specific runtime parameters
    RunPar::subRoot = String("./subRoot");         ///< Pathname of this program's root directory
    RunPar::disposeConfig = String("");            ///< Pathname of the product-disposition file
    RunPar::p2pTimeout = std::chrono::seconds(30); ///< Timeout for connecting to remote P2P servers
    /// Time to wait before re-connecting to the publisher
    RunPar::retryInterval = std::chrono::minutes(1);

    opterr = 0;    // 0 => getopt() won't write to `stderr`
    int c;
    while ((c = ::getopt(argc, argv, RUNPAR_COMMON_OPTIONS_STRING ":c:d:i:r:")) != -1) {
        switch (c) {
            // Common options:
            RUNPAR_COMMON_OPTIONS_CASES(usage)

            // Subscriber-specific options:
            case 'c': {
                try {
                    RunPar::setFromYaml(optarg); // Sets common runtime parameters
                    setFromConfig(optarg);       // Sets program-specific runtime parameters
                }
                catch (const std::exception& ex) {
                    std::throw_with_nested(INVALID_ARGUMENT(
                            String("Couldn't initialize using configuration-file \"") + optarg +
                            "\""));
                }
                break;
            }
            case 'd': {
                RunPar::disposeConfig = String(optarg);
                break;
            }
            case 'i': {
                int seconds;
                if (::sscanf(optarg, "%d", &seconds) != 1)
                    throw INVALID_ARGUMENT(String("Invalid \"-") + static_cast<char>(c) +
                            "\" option argument");
                RunPar::retryInterval = std::chrono::seconds(60);
                break;
            }
            case 'r': {
                RunPar::subRoot = String(optarg);
                break;
            }
        } // `switch` statement
    } // While getopt() loop

    if (!RunPar::pubSrvrAddr) {
        if (argv[optind] == nullptr)
            throw INVALID_ARGUMENT("Publisher's socket address wasn't specified");
        RunPar::pubSrvrAddr = SockAddr(argv[optind++], DEF_PORT);
    }

    if (optind != argc)
        throw INVALID_ARGUMENT("Excess arguments were specified");

    /*
     * If the Internet address for the P2P server wasn't specified, then it's determined from the
     * interface used to connect to the publisher's server.
     */
    if (!RunPar::p2pSrvrAddr)
        RunPar::p2pSrvrAddr = SockAddr(UdpSock(RunPar::pubSrvrAddr).getLclAddr().getInetAddr());

    vetRunPars();
}

/// Sets the exception thrown on an internal thread.
static void setException()
{
    threadEx.set();
}

/**
 * Subscribes to a feed.
 *   - Connects to the publisher's server; and
 *   - Obtains subscription information.
 * @param[in]  p2pSrvrAddr  Socket address of the local P2P server
 * @param[out] subInfo      Subscription information
 * @throw RuntimeError      Couldn't connect to publisher's server at this time
 * @throw RuntimeError      Couldn't send to publisher
 * @throw SystemError       System failure
 * @see publish.cpp:servSubscriber()
 */
static void subscribe(
        SockAddr  p2pSrvrAddr,
        SubInfo&  subInfo)
{
    // Keep consonant with `publish.cpp:servSubscriber()`

    Xprt xprt{TcpClntSock(RunPar::pubSrvrAddr)}; // RAII object

    LOG_INFO("Created publisher-transport " + xprt.to_string());

    P2pSrvrInfo subP2pSrvrInfo{p2pSrvrAddr, (RunPar::maxNumPeers+1)/2};

    // Ensure that tracker to be sent contains local P2P server information
    if (!subP2pSrvrInfo.write(xprt))
        throw RUNTIME_ERROR("Couldn't send P2P server information " + subP2pSrvrInfo.to_string() +
                " to publisher " + RunPar::pubSrvrAddr.to_string());

    if (!subTracker.write(xprt))
        throw RUNTIME_ERROR("Couldn't send tracker " + subTracker.to_string() + " to publisher " +
                RunPar::pubSrvrAddr.to_string());
    LOG_DEBUG("Sent tracker " + subTracker.to_string() + " to publisher " +
            RunPar::pubSrvrAddr.to_string());

    if (!subInfo.read(xprt))
        throw RUNTIME_ERROR("Couldn't receive subscription information from publisher " +
                RunPar::pubSrvrAddr.to_string());
    LOG_NOTE("Received subscription information " + subInfo.to_string());

    subInfo.tracker.insert(subTracker);        // Good if `subTracker` has fewer entries
    subTracker = subInfo.tracker;              // Update official tracker
    DataSeg::setMaxSegSize(subInfo.maxSegSize);

    /**
     * The interface that's used to receive the multicast is the same interface that would be used
     * to send to the multicast source.
     */
    RunPar::mcastIface = UdpSock(SockAddr(subInfo.mcast.srcAddr)).getLclAddr().getInetAddr();
    //LOG_DEBUG("Set interface for multicast reception to " +
            //runPar.mcastIface.to_string());
}

/**
 * Handles a termination signal.
 *
 * @param[in] sig  Signal number. Ignored.
 */
static void sigHandler(const int sig)
{
    done = true;
}

/**
 * Sets the signal handler.
 */
static void setSigHand()
{
    log_setLevelSignal(SIGUSR2);

    // NB: System calls are *not* restarted
    struct sigaction sigact = {};
    (void)sigemptyset(&sigact.sa_mask);
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
        setException();
    }
}

static void stopSubNode(
        SubNodePtr   subNode,
        std::thread& nodeThread)
{
    if (nodeThread.joinable()) {
        subNode->halt(); // Idempotent
        nodeThread.join();
    }
}

static void printMetrics(SubNodePtr subNode)
{
    long numMcastOrig;
    long numP2pOrig;
    long numMcastDup;
    long numP2pDup;
    subNode->getPduCounts(numMcastOrig, numP2pOrig, numMcastDup, numP2pDup);
    LOG_NOTE("Metrics:");
    LOG_NOTE("    Number of multicast PDUs:");
    LOG_NOTE("        Original:  " + std::to_string(numMcastOrig));
    LOG_NOTE("        Duplicate: " + std::to_string(numMcastDup));
    LOG_NOTE("    Number of P2P PDUs:");
    LOG_NOTE("        Original:  " + std::to_string(numP2pOrig));
    LOG_NOTE("        Duplicate: " + std::to_string(numP2pDup));

    const auto totalProds = subNode->getTotalProds();
    if (totalProds == 0) {
        LOG_NOTE("Total:");
        LOG_NOTE("    Products: " + std::to_string(totalProds));
    }
    else {
        const auto totalBytes = subNode->getTotalBytes();
        const auto meanProdSize = static_cast<double>(totalBytes)/totalProds;
        const auto totalLatency = subNode->getTotalLatency();
        const auto byteRate = totalBytes/totalLatency;
        LOG_NOTE("    Total Number of:");
        LOG_NOTE("        Products: " + std::to_string(totalProds));
        LOG_NOTE("        Bytes:    " + std::to_string(totalBytes));
        LOG_NOTE("    Mean Product:");
        LOG_NOTE("        Size:    " + std::to_string(meanProdSize) + " bytes");
        LOG_NOTE("        Latency: " + std::to_string(totalLatency/totalProds) + " s");
    }
}

/**
 * Executes a session.
 * @throw RuntimeError  Couldn't connect to publisher's server at this time
 */
static void trySession()
{
    // Create peer-connection server
    auto peerConnSrvr = PeerConnSrvr::create();

    // Use the address of the peer-connection server because port number might have been 0
    SubInfo subInfo; // Subscription information
    subscribe(peerConnSrvr->getSrvrAddr(), subInfo);

    RunPar::feedName     = subInfo.feedName;
    RunPar::maxSegSize   = subInfo.maxSegSize;
    RunPar::mcastDstAddr = subInfo.mcast.dstAddr;
    RunPar::mcastSrcAddr = subInfo.mcast.srcAddr;
    RunPar::prodKeepTime = subInfo.keepTime;
    RunPar::maxSegSize   = subInfo.maxSegSize;

    subTracker.insert(subInfo.tracker);        // Update tracker
    DataSeg::setMaxSegSize(RunPar::maxSegSize);

    /**
     * The interface that's used to receive the multicast is the same interface that would be used
     * to send to the multicast source.
     */
    RunPar::mcastIface = UdpSock(SockAddr(subInfo.mcast.srcAddr)).getLclAddr().getInetAddr();
    //LOG_DEBUG("Set interface for multicast reception to " +
            //runPar.mcastIface.to_string());

    // Create disposer factory-method
    Disposer::Factory factory = [&] (const LastProdPtr& lastProcessed) {
        return RunPar::disposeConfig.size()
            ? Disposer::createFromYaml(RunPar::disposeConfig, lastProcessed)
            : Disposer{}; // No local processing <=> invalid instance
    };

    //LOG_DEBUG("Creating subscribing node");
    auto    subNode = SubNode::create(subInfo, peerConnSrvr, factory, nullptr);

    try {
        subNode->run();
        if (threadEx)
            LOG_DEBUG("Internal thread threw exception");
        threadEx.throwIfSet(); // Throws if failure on a thread
    } // Disposer started
    catch (const std::exception& ex) {
        LOG_DEBUG(ex);
        throw;
    }

    printMetrics(subNode);
}

/**
 * Main entry point.
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
    int status = 0;

    std::set_terminate(&terminate); // NB: Bicast version

    try {
        log_setName(::basename(argv[0]));
        LOG_NOTE("Starting up: " + getCmdLine(argc, argv));

        setRunPars(argc, argv);
        subTracker = Tracker(RunPar::trackerCap);
        done = false;
        setSigHand(); // Catches termination signals

        while (!done) {
            try {
                trySession();
            }
            catch (const RuntimeError& ex) {
                LOG_WARN(ex);
            }
            if (!done) {
                LOG_INFO("Sleeping " + std::to_string(RunPar::retryInterval));
                ::sleep(std::chrono::duration_cast<std::chrono::seconds>(
                        RunPar::retryInterval).count());
            }
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
