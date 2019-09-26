/**
 * This file tests classes hycast::Shipping and hycast::Receiving.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ShipRecv_test.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "Interface.h"
#include "P2pMgr.h"
#include "PerfMeter.h"
#include "Processing.h"
#include "Receiving.h"
#include "Shipping.h"
#include "Thread.h"
#include "YamlPeerSource.h"

#include <gtest/gtest.h>
#include <mutex>

namespace {

// The fixture for testing classes Shipping and Receiving.
class ShipRecvTest : public ::testing::Test, public hycast::Processing
{
protected:
    void process(hycast::Product prod)
    {
        // Parentheses must be used in the following statement
        static std::vector<bool>    rcvdProdIndexes(NUM_PRODUCTS, false);
        static std::mutex           mutex;
        static unsigned long        rcvdUniqueProds = 0;
        static int                  seqIndex{0};
        std::lock_guard<std::mutex> lock{mutex};
        auto                        info = prod.getInfo();
        size_t                      prodIndex =
                static_cast<uint64_t>(info.getIndex());
        perfMeter.product(info);
        LOG_INFO("product#=%d, prodIndex=%zu", seqIndex++, prodIndex);
        if (rcvdProdIndexes.at(prodIndex))
            throw hycast::LOGIC_ERROR("Duplicate product: index=" +
                    std::to_string(prodIndex));
        rcvdProdIndexes[prodIndex] = true;
        if (++rcvdUniqueProds == NUM_PRODUCTS)
            cue.cue();
    }

    ShipRecvTest()
    {
        // gcc 4.8 doesn't support non-trivial designated initializers
        srcMcastInfo.mcastAddr = mcastAddr;
        srcMcastInfo.srcAddr = localInetAddr;

        for (size_t i = 0; i < sizeof(data); ++i)
            data[i] = i % UCHAR_MAX;
    }

    const double               drop = 0.2;
    const int                  NUM_PRODUCTS = 100;
    char                       data[100000];
    hycast::ProdStore          prodStore{};
    const in_port_t            srcPort{38800};
    const in_port_t            snkPort{38801};
    const hycast::InetSockAddr mcastAddr{"232.0.0.0", srcPort};
    const hycast::InetAddr     localInetAddr{
            hycast::Interface{ETHNET_IFACE_NAME}.getInetAddr(AF_INET)};
    hycast::InetSockAddr       srcSrvrAddr{localInetAddr, srcPort};
    hycast::InetSockAddr       snkSrvrAddr{localInetAddr, snkPort};
    const unsigned             protoVers{0};
    hycast::McastSender        mcastSender{mcastAddr, protoVers};
    hycast::YamlPeerSource     peerSource{"[{inetAddr: " +
            localInetAddr.to_string() + ", port: " + std::to_string(srcPort) +
            "}]"};
    hycast::ProdIndex          prodIndex{0};
    const unsigned             maxPeers = 1;
    const unsigned             stasisDuration =
            hycast::PeerSet::defaultStasisDuration;
    // gcc 4.8 doesn't support non-trivial designated initializers
    hycast::P2pInfo            p2pInfo{snkSrvrAddr, maxPeers, peerSource,
            stasisDuration};
    // gcc 4.8 doesn't support non-trivial designated initializers
    hycast::SrcMcastInfo       srcMcastInfo;
    hycast::PerfMeter          perfMeter{};
    hycast::Cue                cue{};
};

// Tests shipping construction
TEST_F(ShipRecvTest, ShippingConstruction) {
    hycast::Shipping(prodStore, mcastAddr, protoVers, srcSrvrAddr);
}

// Tests receiving construction
TEST_F(ShipRecvTest, ReceivingConstruction) {
    hycast::Receiving{srcMcastInfo, p2pInfo, *this, protoVers};
}

// Tests shipping and receiving products
TEST_F(ShipRecvTest, ShippingAndReceiving) {
    auto logLevelOnEntry = hycast::logThreshold;
    hycast::logThreshold = hycast::LOG_LEVEL_NOTE;
    // Create shipper
    hycast::Shipping shipping{prodStore, mcastAddr, protoVers, srcSrvrAddr,
            maxPeers, stasisDuration};

    // Create receiver
    hycast::Receiving receiving{srcMcastInfo, p2pInfo, *this, protoVers, "",
        drop};

    ::sleep(1);

    // Ship products
    for (hycast::ProdIndex i = 0; NUM_PRODUCTS > i; ++i) {
        std::string name = std::string{"product " } + std::to_string(i);
        hycast::MemoryProduct prod{i, name, sizeof(data), data};
        shipping.ship(prod);
    }

    cue.wait();
    perfMeter.stop();
    std::cout << perfMeter << '\n';
    hycast::logThreshold = logLevelOnEntry;
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
