/**
 * This file tests the class `Shipping`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Shipping.cpp
 * @author: Steven R. Emmerson
 */

#include "Shipping.h"
#include "YamlPeerSource.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class Shipping.
class ShippingTest : public ::testing::Test {
protected:
	ShippingTest()
	{}

	hycast::PeerSet            peerSet{[](hycast::Peer&){}};
    hycast::ProdStore          prodStore{};
    const in_port_t            port{38800};
    const hycast::InetSockAddr mcastAddr{"232.0.0.0", port};
    const hycast::InetAddr     serverInetAddr{"192.168.132.128"};
    hycast::InetSockAddr       serverAddr{serverInetAddr, port};
    const unsigned             protoVers{0};
    hycast::McastSender        mcastSender{mcastAddr, protoVers};
    hycast::YamlPeerSource     peerSource{"[{inetAddr: " +
    	serverInetAddr.to_string() + ", port: " + std::to_string(port) + "}]"};
};

// Tests construction
TEST_F(ShippingTest, Construction) {
    hycast::Shipping(prodStore, mcastSender, peerSet, serverAddr);
}

// Tests shipping a product
TEST_F(ShippingTest, Shipping) {
    hycast::ProdIndex prodIndex(0);
    char              data[128000];
    ::memset(data, 0xbd, sizeof(data));
    hycast::Product   prod("product", prodIndex, data, sizeof(data));
    hycast::Shipping  shipping{prodStore, mcastSender, peerSet, serverAddr};
    shipping.ship(prod);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
