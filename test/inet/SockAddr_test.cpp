/**
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: SockAddr_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file tests class `SockAddr`.
 */

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <main/inet/SockAddr.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <string.h>
#include <sys/socket.h>

namespace {

// The fixture for testing class SockAddr.
class SockAddrTest : public ::testing::Test {
protected:
    #define PORT1 1
    #define PORT1_STR "1"
    #define PORT2 2
    #define PORT2_STR "2"

    #define IPV4_HOST1 "128.117.140.56"
    #define IPV4_HOST2 "128.117.140.57"

    #define IPV6_HOST1 "2001:db8::ff00:42"
    #define IPV6_HOST2 "2002:db9::ff01:43"

    #define NAME_HOST1 "idd.unidata.ucar.edu"
    #define NAME_HOST2 "www.unidata.ucar.edu"

    #define STRINGIZE(x) #x

    const char* sockAddrInSpec_1;
    const char* sockAddrInSpec_2;
    const char* sockAddrIn6Spec_1;
    const char* sockAddrIn6Spec_2;
    const char* sockAddrNameSpec_1;
    const char* sockAddrNameSpec_2;

    hycast::SockAddr sockAddrIn_1;
    hycast::SockAddr sockAddrIn_2;
    hycast::SockAddr sockAddrIn6_1;
    hycast::SockAddr sockAddrIn6_2;
    hycast::SockAddr sockAddrName_1;
    hycast::SockAddr sockAddrName_2;

    SockAddrTest()
        : sockAddrInSpec_1{IPV4_HOST1 ":" PORT1_STR}
        , sockAddrInSpec_2{IPV4_HOST2 ":" PORT2_STR}
        , sockAddrIn6Spec_1{"[" IPV6_HOST1 "]:" PORT1_STR}
        , sockAddrIn6Spec_2{"[" IPV6_HOST2 "]:" PORT2_STR}
        , sockAddrNameSpec_1{NAME_HOST1 ":" PORT1_STR}
        , sockAddrNameSpec_2{NAME_HOST2 ":" PORT1_STR}
        , sockAddrIn_1{}
        , sockAddrIn_2{}
        , sockAddrIn6_1{}
        , sockAddrIn6_2{}
        , sockAddrName_1{}
        , sockAddrName_2{}
    {
        // IPv4 socket addresses
        struct in_addr inAddr{::inet_addr(IPV4_HOST1)};

        sockAddrIn_1 = hycast::SockAddr(inAddr, PORT1);

        inAddr.s_addr = ::inet_addr(IPV4_HOST2);
        sockAddrIn_2 = hycast::SockAddr(inAddr, PORT2);

        // IPv6 socket addresses
        struct in6_addr in6Addr;
        inet_pton(AF_INET6, IPV6_HOST1, &in6Addr);

        sockAddrIn6_1 = hycast::SockAddr(in6Addr, PORT1);

        inet_pton(AF_INET6, IPV6_HOST2, &in6Addr);
        sockAddrIn6_2 = hycast::SockAddr(in6Addr, PORT2);

        // Hostname socket addresses
        sockAddrName_1 = hycast::SockAddr{NAME_HOST1, PORT1};
        sockAddrName_2 = hycast::SockAddr{NAME_HOST2, PORT2};
    }
};

TEST_F(SockAddrTest, DefaultConstruction) {
    hycast::SockAddr sockAddr{}; // Braces are necessary
    EXPECT_FALSE(sockAddr);
}

TEST_F(SockAddrTest, BadSpec) {
    EXPECT_THROW(hycast::SockAddr("#"), std::invalid_argument);
    EXPECT_THROW(hycast::SockAddr("127.0.0.1:99999"), std::invalid_argument);
    EXPECT_THROW(hycast::SockAddr("localhost:999999"), std::invalid_argument);
    EXPECT_THROW(hycast::SockAddr("[ax:zz]:1"), std::invalid_argument);
}

// Tests construction of an IPv4 socket address
TEST_F(SockAddrTest, IPv4Construction) {
    hycast::SockAddr sockAddr{sockAddrInSpec_1};

    EXPECT_TRUE(sockAddr);

    const std::string actual{sockAddr.to_string()};
    EXPECT_STREQ(sockAddrInSpec_1, actual.data());
    EXPECT_TRUE((sockAddr < sockAddrIn_2) != (sockAddrIn_2 < sockAddr));
}

// Tests construction of an IPv6 socket address
TEST_F(SockAddrTest, IPv6Construction) {
    hycast::SockAddr sockAddr{sockAddrIn6Spec_1};

    EXPECT_TRUE(sockAddr);

    const std::string actual{sockAddr.to_string()};
    EXPECT_STREQ(sockAddrIn6Spec_1, actual.data());
    EXPECT_TRUE((sockAddr < sockAddrIn6_2) != (sockAddrIn6_2 < sockAddr));
}

// Tests construction of a named socket address
TEST_F(SockAddrTest, NameConstruction) {
    hycast::SockAddr sockAddr{sockAddrNameSpec_1};

    EXPECT_TRUE(sockAddr);

    EXPECT_STREQ(sockAddrNameSpec_1, sockAddr.to_string().data());
    EXPECT_TRUE((sockAddr < sockAddrName_2) != (sockAddrName_2 < sockAddr));
}

// Tests less-than operator
TEST_F(SockAddrTest, LessThanOperator) {
    EXPECT_TRUE(sockAddrIn_1 < sockAddrIn_2);
    EXPECT_FALSE(sockAddrIn_2 < sockAddrIn_1);

    EXPECT_TRUE(sockAddrIn_1 < sockAddrIn6_1);
    EXPECT_FALSE(sockAddrIn6_1 < sockAddrIn_1);

    EXPECT_TRUE(sockAddrIn_1 < sockAddrName_1);
    EXPECT_FALSE(sockAddrName_1 < sockAddrIn_1);

    EXPECT_TRUE(sockAddrIn6_1 < sockAddrIn6_2);
    EXPECT_FALSE(sockAddrIn6_2 < sockAddrIn6_1);

    EXPECT_TRUE(sockAddrIn6_1 < sockAddrName_1);
    EXPECT_FALSE(sockAddrName_1 < sockAddrIn6_1);

    EXPECT_TRUE(sockAddrName_1 < sockAddrName_2);
    EXPECT_FALSE(sockAddrName_2 < sockAddrName_1);

    hycast::SockAddr sockAddrIn = sockAddrIn_1.clone(PORT1+1);
    EXPECT_TRUE(sockAddrIn_1 < sockAddrIn);
    EXPECT_FALSE(sockAddrIn < sockAddrIn_1);

    hycast::SockAddr sockAddrIn6 = sockAddrIn6_1.clone(PORT1+1);
    EXPECT_TRUE(sockAddrIn6_1 < sockAddrIn6);
    EXPECT_FALSE(sockAddrIn6 < sockAddrIn6_1);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
