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
#include "Processing.h"
#include "ProdStore.h"
#include "Receiving.h"
#include "Shipping.h"
#include "YamlPeerSource.h"

#include <gtest/gtest.h>
#include <iostream>
#include <climits>

namespace {

// The fixture for testing class Receiving.
class ShipRecvTest : public ::testing::Test, public hycast::Processing
{
protected:
    void process(hycast::Product prod)
    {
        EXPECT_EQ(this->prod, prod);
    }

    ShipRecvTest()
        : p2pInfo{serverAddr, maxPeers, peerSource, stasisDuration}
    {
        // gcc 4.8 doesn't support non-trivial designated initializers
        srcMcastInfo.mcastAddr = mcastAddr;
        srcMcastInfo.srcAddr = serverInetAddr;

        unsigned char data[128000];
        for (size_t i = 0; i < sizeof(data); ++i)
                data[i] = i % UCHAR_MAX;

        prod = hycast::Product{"product", prodIndex, data, sizeof(data)};
    }

    hycast::PeerSet                 peerSet{1};
    hycast::ProdStore               prodStore{};
    const in_port_t                 port{38800};
    const hycast::InetSockAddr      mcastAddr{"232.0.0.0", port};
    const hycast::InetAddr          serverInetAddr{
            hycast::Interface{ETHNET_IFACE_NAME}.getInetAddr(AF_INET)};
    hycast::InetSockAddr            serverAddr{serverInetAddr, port};
    const unsigned                  protoVers{0};
    hycast::McastSender             mcastSender{mcastAddr, protoVers};
    hycast::YamlPeerSource          peerSource{"[{inetAddr: " +
            serverInetAddr.to_string() + ", port: " + std::to_string(port) +
            "}]"};
    hycast::ProdIndex               prodIndex{0};
    const unsigned                  maxPeers = 1;
    const hycast::PeerSet::TimeUnit stasisDuration{2};
    // gcc 4.8 doesn't support non-trivial designated initializers
    hycast::P2pInfo                 p2pInfo;
    // gcc 4.8 doesn't support non-trivial designated initializers
    hycast::SrcMcastInfo            srcMcastInfo;
    hycast::Product                 prod{};
};

// Tests shipping construction
TEST_F(ShipRecvTest, ShippingConstruction) {
    hycast::Shipping(prodStore, mcastAddr, protoVers, serverAddr);
}

// Tests receiving construction
TEST_F(ShipRecvTest, ReceivingConstruction) {
    hycast::Receiving{srcMcastInfo, p2pInfo, *this, protoVers};
}

// Tests shipping and receiving a product
TEST_F(ShipRecvTest, ShippingAndReceiving) {
	// Create shipper
    hycast::Shipping shipping{prodStore, mcastAddr, protoVers, maxPeers,
    	stasisDuration, serverAddr};

    ::usleep(100000);

    // Create receiver
    hycast::Receiving receiving{srcMcastInfo, p2pInfo, *this, protoVers};

    ::usleep(100000);

    // Ship product
    shipping.ship(prod);

    ::usleep(100000);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
