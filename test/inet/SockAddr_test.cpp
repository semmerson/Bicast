/**
 * This file tests class `SockAddr`.
 *
 *   @file: SockAddr_test.cpp
 * @author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
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

#include "SockAddr.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <string.h>
#include <sys/socket.h>

namespace {

using namespace bicast;

// The fixture for testing class SockAddr.
class SockAddrTest : public testing::Test {
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

    SockAddr sockAddrIn_1;
    SockAddr sockAddrIn_2;
    SockAddr sockAddrIn6_1;
    SockAddr sockAddrIn6_2;
    SockAddr sockAddrName_1;
    SockAddr sockAddrName_2;

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

        sockAddrIn_1 = SockAddr(inAddr, PORT1);

        inAddr.s_addr = ::inet_addr(IPV4_HOST2);
        sockAddrIn_2 = SockAddr(inAddr, PORT2);

        // IPv6 socket addresses
        struct in6_addr in6Addr;
        inet_pton(AF_INET6, IPV6_HOST1, &in6Addr);

        sockAddrIn6_1 = SockAddr(in6Addr, PORT1);

        inet_pton(AF_INET6, IPV6_HOST2, &in6Addr);
        sockAddrIn6_2 = SockAddr(in6Addr, PORT2);

        // Hostname socket addresses
        sockAddrName_1 = SockAddr{NAME_HOST1, PORT1};
        sockAddrName_2 = SockAddr{NAME_HOST2, PORT2};
    }
};

TEST_F(SockAddrTest, DefaultConstruction) {
    SockAddr sockAddr{}; // Braces are necessary
    EXPECT_FALSE(sockAddr);
}

TEST_F(SockAddrTest, BadSpec) {
    EXPECT_THROW(SockAddr("#"), std::invalid_argument);
    EXPECT_THROW(SockAddr("127.0.0.1:99999"), std::invalid_argument);
    EXPECT_THROW(SockAddr("localhost:999999"), std::invalid_argument);
    EXPECT_THROW(SockAddr("[ax:zz]:1"), std::invalid_argument);
}

// Tests construction of an IPv4 socket address
TEST_F(SockAddrTest, IPv4Construction) {
    SockAddr sockAddr{sockAddrInSpec_1};

    EXPECT_TRUE(sockAddr);
    EXPECT_FALSE(sockAddr.getInetAddr().isLinkLocal()); // Should be in "InetAddr_test.cpp"

    const std::string actual(sockAddr.to_string());
    EXPECT_STREQ(sockAddrInSpec_1, actual.data());
    EXPECT_TRUE((sockAddr < sockAddrIn_2) != (sockAddrIn_2 < sockAddr));
}

/*
 * Tests construction of link-local IPv4 socket addresses. This should be in "InetAddr_test.cpp" but
 * that doesn't exist, so it's here.
 */
TEST_F(SockAddrTest, LinkLocalIPv4) {
    SockAddr sockAddr1{"192.168.0.1:38800"};
    EXPECT_TRUE(sockAddr1);
    EXPECT_TRUE(sockAddr1.getInetAddr().isLinkLocal());

    SockAddr sockAddr2{"10.0.0.1:38800"};
    EXPECT_TRUE(sockAddr2);
    EXPECT_TRUE(sockAddr2.getInetAddr().isLinkLocal());
}

// Tests construction of an IPv6 socket address
TEST_F(SockAddrTest, IPv6Construction) {
    SockAddr sockAddr{sockAddrIn6Spec_1};

    EXPECT_TRUE(sockAddr);
    EXPECT_FALSE(sockAddr.getInetAddr().isLinkLocal()); // Should be in "InetAddr_test.cpp"

    const std::string actual(sockAddr.to_string());
    EXPECT_STREQ(sockAddrIn6Spec_1, actual.data());
    EXPECT_TRUE((sockAddr < sockAddrIn6_2) != (sockAddrIn6_2 < sockAddr));
}

/*
 * Tests construction of a link-local IPv6 socket address. This should be in "InetAddr_test.cpp" but
 * that doesn't exist, so it's here.
 */
TEST_F(SockAddrTest, LinkLocalIPv6) {
    SockAddr sockAddr{"[fe80::200:5aee:feaa:20a2]:38800"};
    EXPECT_TRUE(sockAddr);
    EXPECT_TRUE(sockAddr.getInetAddr().isLinkLocal());
}

// Tests construction of a named socket address
TEST_F(SockAddrTest, NameConstruction) {
    SockAddr sockAddr{sockAddrNameSpec_1};

    EXPECT_TRUE(sockAddr);

    EXPECT_STREQ(sockAddrNameSpec_1, sockAddr.to_string().data());
    EXPECT_TRUE((sockAddr < sockAddrName_2) != (sockAddrName_2 < sockAddr));
}

// Tests construction given a default port number
TEST_F(SockAddrTest, DefaultPortNumber) {
    auto sockAddr = SockAddr{IPV4_HOST1};
    EXPECT_TRUE(sockAddr);
    EXPECT_STREQ(IPV4_HOST1, sockAddr.getInetAddr().to_string().data());
    EXPECT_EQ(0, sockAddr.getPort());

    sockAddr = SockAddr{IPV4_HOST1, 1};
    EXPECT_TRUE(sockAddr);
    EXPECT_STREQ(IPV4_HOST1, sockAddr.getInetAddr().to_string().data());
    EXPECT_EQ(1, sockAddr.getPort());

    sockAddr = SockAddr{IPV6_HOST1};
    EXPECT_TRUE(sockAddr);
    EXPECT_STREQ(IPV6_HOST1, sockAddr.getInetAddr().to_string().data());
    EXPECT_EQ(0, sockAddr.getPort());

    sockAddr = SockAddr{IPV6_HOST1, 1};
    EXPECT_TRUE(sockAddr);
    EXPECT_STREQ(IPV6_HOST1, sockAddr.getInetAddr().to_string().data());
    EXPECT_EQ(1, sockAddr.getPort());

    sockAddr = SockAddr{NAME_HOST1};
    EXPECT_TRUE(sockAddr);
    EXPECT_STREQ(NAME_HOST1, sockAddr.getInetAddr().to_string().data());
    EXPECT_EQ(0, sockAddr.getPort());

    sockAddr = SockAddr{NAME_HOST1, 1};
    EXPECT_TRUE(sockAddr);
    EXPECT_STREQ(NAME_HOST1, sockAddr.getInetAddr().to_string().data());
    EXPECT_EQ(1, sockAddr.getPort());
}

// Tests hashing
TEST_F(SockAddrTest, Hash) {
    const auto   myHash = std::hash<SockAddr>{};

    size_t h1 = myHash(sockAddrIn_1);
    size_t h2 = myHash(sockAddrIn_2);
    EXPECT_NE(h1, h2);

    h1 = myHash(sockAddrIn_1);
    h2 = myHash(sockAddrName_1);
    EXPECT_NE(h1, h2);

    h1 = myHash(sockAddrIn6_1 );
    h2 = myHash(sockAddrIn6_2);
    EXPECT_NE(h1, h2);

    h1 = myHash(sockAddrIn6_1 );
    h2 = myHash(sockAddrName_1);
    EXPECT_NE(h1, h2);

    h1 = myHash(sockAddrName_1 );
    h2 = myHash(sockAddrName_2);
    EXPECT_NE(h1, h2);

    SockAddr sockAddrIn = sockAddrIn_1.clone(PORT1+1);
    h1 = myHash(sockAddrIn_1 );
    h2 = myHash(sockAddrIn);
    EXPECT_NE(h1, h2);

    SockAddr sockAddrIn6 = sockAddrIn6_1.clone(PORT1+1);
    h1 = myHash(sockAddrIn6_1 );
    h2 = myHash(sockAddrIn6);
    EXPECT_NE(h1, h2);
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

    SockAddr sockAddrIn = sockAddrIn_1.clone(PORT1+1);
    EXPECT_TRUE(sockAddrIn_1 < sockAddrIn);
    EXPECT_FALSE(sockAddrIn < sockAddrIn_1);

    SockAddr sockAddrIn6 = sockAddrIn6_1.clone(PORT1+1);
    EXPECT_TRUE(sockAddrIn6_1 < sockAddrIn6);
    EXPECT_FALSE(sockAddrIn6 < sockAddrIn6_1);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
