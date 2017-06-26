/**
 * This file tests class `Interface`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Interface_test.cpp
 * Created On: May 9, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "Interface.h"

#include <iostream>
#include <gtest/gtest.h>

namespace {

/// The fixture for testing class `Interface`
class InterfaceTest : public ::testing::Test
{
protected:
    const std::string name{"ens33"};
};

// Tests default construction
TEST_F(InterfaceTest, DefaultConstruction)
{
    hycast::Interface iface{};
}

// Tests construction from name
TEST_F(InterfaceTest, ConstructionFromName)
{
    hycast::Interface iface{name};
}

// Tests getting IPv4 address
TEST_F(InterfaceTest, GetIPv4Addr)
{
    hycast::Interface iface{name};
    auto inetAddr = iface.getInetAddr(AF_INET);
    std::cout << "IPv4 addr = " << inetAddr.to_string() << '\n';
}

// Tests getting IPv6 address
TEST_F(InterfaceTest, GetIPv6Addr)
{
    hycast::Interface iface{name};
    auto inetAddr = iface.getInetAddr(AF_INET6);
    std::cout << "IPv6 addr = " << inetAddr.to_string() << '\n';
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
