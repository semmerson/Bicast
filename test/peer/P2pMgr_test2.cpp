#include "SockAddr.h"
#include "InAddr.h"

#include <gtest/gtest.h>
#include <iostream>

namespace {

class P2pMgrTest : public ::testing::Test
{
protected:
    hycast::SockAddr srcAddr;
    hycast::InAddr inAddr;
    int five;

    P2pMgrTest() {
    }

    void SetUp() override {
        srcAddr = hycast::SockAddr("localhost:38800");
        inAddr = hycast::InAddr("localhost");
        five = 5;
    }
};

TEST_F(P2pMgrTest, DataExchange) {
    std::cout << "srcAddr: " << srcAddr.to_string() << '\n';
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
