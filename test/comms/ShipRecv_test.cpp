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

#include "Interface.h"
#include "McastSender.h"
#include "P2pMgr.h"
#include "PeerSet.h"
#include "PerfMeter.h"
#include "Processing.h"
#include "ProdIndex.h"
#include "ProdStore.h"
#include "Receiving.h"
#include "Shipping.h"
#include "Thread.h"
#include "YamlPeerSource.h"

#include <gtest/gtest.h>
#include <iostream>
#include <climits>

namespace {

// The fixture for testing classes Shipping and Receiving.
class ShipRecvTest : public ::testing::Test, public hycast::Processing
{
protected:
    void process(hycast::Product prod)
    {
        static int n = 0;
        auto info = prod.getInfo();
        perfMeter.product(info);
        LOG_INFO("product#=%d, prodIndex=%s", n,
                std::to_string(info.getIndex()).c_str());
        if (++n == NUM_PRODUCTS)
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

    const double                    drop = 0.0;
    const int                       NUM_PRODUCTS = 50;
    unsigned char                   data[10000];
    //unsigned char                   data[1];
    hycast::ProdStore               prodStore{};
    hycast::PeerSet                 peerSet{prodStore, 1};
    const in_port_t                 srcPort{38800};
    const in_port_t                 snkPort{38801};
    const hycast::InetSockAddr      mcastAddr{"232.0.0.0", srcPort};
    const hycast::InetAddr          localInetAddr{
            hycast::Interface{ETHNET_IFACE_NAME}.getInetAddr(AF_INET)};
    hycast::InetSockAddr            srcSrvrAddr{localInetAddr, srcPort};
    hycast::InetSockAddr            snkSrvrAddr{localInetAddr, snkPort};
    const unsigned                  protoVers{0};
    hycast::McastSender             mcastSender{mcastAddr, protoVers};
    hycast::YamlPeerSource          peerSource{"[{inetAddr: " +
            localInetAddr.to_string() + ", port: " + std::to_string(srcPort) +
            "}]"};
    hycast::ProdIndex               prodIndex{0};
    const unsigned                  maxPeers = 1;
    const hycast::PeerSet::TimeUnit stasisDuration{2};
    // gcc 4.8 doesn't support non-trivial designated initializers
    hycast::P2pInfo                 p2pInfo{snkSrvrAddr, maxPeers, peerSource,
            stasisDuration};
    // gcc 4.8 doesn't support non-trivial designated initializers
    hycast::SrcMcastInfo            srcMcastInfo;
    hycast::PerfMeter               perfMeter{};
    hycast::Cue                     cue{};
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
    hycast::logLevel = hycast::LOG_INFO;
    // Create shipper
    hycast::Shipping shipping{prodStore, mcastAddr, protoVers, maxPeers,
    	stasisDuration, srcSrvrAddr};

    // Create receiver
    hycast::Receiving receiving{srcMcastInfo, p2pInfo, *this, protoVers, "",
        drop};

    ::sleep(1);

    // Ship products
    for (hycast::ProdIndex i = 0; NUM_PRODUCTS > i; ++i) {
        std::string name = std::string{"product " } + std::to_string(i);
        hycast::Product prod{name, i, data, sizeof(data)};
        shipping.ship(prod);
    }

    cue.wait();
    perfMeter.stop();
    std::cout << perfMeter << '\n';
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
