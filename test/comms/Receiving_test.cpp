/**
 * This file tests class hycast::Receiving.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Receiving_test.cpp
 * @author: Steven R. Emmerson
 */

#include "McastSender.h"
#include "P2pMgr.h"
#include "PeerSet.h"
#include "Processing.h"
#include "ProdStore.h"
#include "Receiving.h"
#include "Shipping.h"
#include "YamlPeerSource.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class Receiving.
class ReceivingTest : public ::testing::Test {
protected:
public:
	class Processing final : public hycast::Processing
	{
	public:
		void process(hycast::Product prod)
		{
		}
	};

	ReceivingTest()
	{
		p2pInfo.peerCount = maxPeers;
		p2pInfo.peerSource = &peerSource;
		p2pInfo.serverSockAddr = serverAddr;
		p2pInfo.stasisDuration = stasisDuration;
	}

	hycast::PeerSet            peerSet{[](hycast::Peer&){}};
    hycast::ProdStore          prodStore{};
    const in_port_t            port{38800};
    const hycast::InetSockAddr mcastAddr{"232.0.0.0", port};
    const std::string          serverInetAddr{"192.168.132.128"};
    hycast::InetSockAddr       serverAddr{serverInetAddr, port};
    const unsigned             protoVers{0};
    hycast::McastSender        mcastSender{mcastAddr, protoVers};
    hycast::YamlPeerSource     peerSource{"[{inetAddr: " + serverInetAddr +
    	    ", port: " + std::to_string(port) + "}]"};
    hycast::ProdIndex          prodIndex{0};
    char                       data[128000];
    const unsigned             maxPeers = 1;
    const unsigned             stasisDuration = 2;
    hycast::P2pInfo            p2pInfo;
    Processing                 processing{};
};

// Tests construction
TEST_F(ReceivingTest, Construction) {
    hycast::Receiving{p2pInfo, processing};
}

// Tests shipping and receiving a product
TEST_F(ReceivingTest, ShippingAndReceiving) {
	// Create shipper
    hycast::Shipping  shipping{prodStore, mcastSender, peerSet, serverAddr};

    ::sleep(1);

    // Create receiver
    const unsigned maxPeers = 1;
    const unsigned stasisDuration = 2;
    hycast::Receiving{p2pInfo, processing};

    ::sleep(1);

    // Ship product
    ::memset(data, 0xbd, sizeof(data));
    hycast::Product   prod("product", prodIndex, data, sizeof(data));
    shipping.ship(prod);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
