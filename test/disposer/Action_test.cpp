/**
 * This file tests class `Action`.
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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
 *       File: Action_test.cpp
 * Created On: Sep 3, 2022
 *     Author: Steven R. Emmerson
 */
#include "config.h"
#include "Action.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <vector>

namespace {

using namespace hycast;

/// The fixture for testing class `Action`
class ActionTest : public ::testing::Test
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ActionTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~ActionTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.
    void initRand(
            char*  bytes,
            size_t nbytes) {
        auto fd = ::open("/dev/urandom", O_RDONLY); // "urandom" is sufficient for testing
        if (fd < 0)
            throw std::system_error(errno, std::generic_category(), "open() failure");

        try {
            while (nbytes > 0) {
                auto nread = ::read(fd, bytes, nbytes);
                if (nread == -1)
                    throw std::system_error(errno, std::generic_category(),
                            "initRand(): read() failure");
                nbytes -= nread;
                bytes += nread;
            }

            ::close(fd);
        } // `fd` open
        catch (const std::exception& ex) {
            ::close(fd);
            throw;
        }
    }
};

// Tests operator bool()
TEST_F(ActionTest, OperatorBool)
{
    Action action;
    EXPECT_FALSE(action);
}

// Tests shouldPersist()
TEST_F(ActionTest, ShouldPersist)
{
    auto action1 = PipeAction(std::vector<String>{"one"}, false);
    EXPECT_FALSE(action1.shouldPersist());

    auto action2 = PipeAction(std::vector<String>{"two"}, true);
    EXPECT_TRUE(action2.shouldPersist());
}

// Tests hash()
TEST_F(ActionTest, Hash)
{
    auto action = PipeAction(std::vector<String>(), false);
    EXPECT_EQ(0, action.hash());

    auto action1 = PipeAction(std::vector<String>{"one"}, false);
    EXPECT_NE(0, action1.hash());

    auto action2 = PipeAction(std::vector<String>{"two"}, false);
    EXPECT_NE(action1.hash(), action2.hash());
}

// Tests operator==()
TEST_F(ActionTest, equals)
{
    auto action1 = PipeAction(std::vector<String>{"one"}, false);
    EXPECT_TRUE(action1 == action1);
    auto action2 = PipeAction(std::vector<String>{"two"}, false);
    EXPECT_FALSE(action1 == action2);
}

// Tests process()
TEST_F(ActionTest, TransientAction)
{
    char expectedData[5000];
    initRand(expectedData, sizeof(expectedData));

    std::vector<String> args{"sh", "-c", "cat >/tmp/Action_test"};
    PipeAction                action(args, false);
    EXPECT_TRUE(action.process(expectedData, sizeof(expectedData)));

    char actualData[sizeof(expectedData)];
    auto fd = ::open("/tmp/Action_test", O_RDONLY);
    EXPECT_NE(-1, fd);
    EXPECT_EQ(sizeof(expectedData), ::read(fd, actualData, sizeof(actualData)));
    ::close(fd);
    EXPECT_EQ(0, ::memcmp(expectedData, actualData, sizeof(actualData)));

    //EXPECT_NE(-1, ::unlink("/tmp/Action_test"));
}

// Tests process()
TEST_F(ActionTest, PersistentAction)
{
    char expectedData[5000];
    initRand(expectedData, sizeof(expectedData));

    std::vector<String> args{"sh", "-c", "cat >/tmp/Action_test"};
    {
        PipeAction action(args, true);
        EXPECT_TRUE(action.process(expectedData, sizeof(expectedData)));
    } // Destruction of `action` should wait on decoder

    char actualData[sizeof(expectedData)];
    auto fd = ::open("/tmp/Action_test", O_RDONLY);
    EXPECT_NE(-1, fd);
    EXPECT_EQ(sizeof(expectedData), ::read(fd, actualData, sizeof(actualData)));
    ::close(fd);
    EXPECT_EQ(0, ::memcmp(expectedData, actualData, sizeof(actualData)));

    //EXPECT_NE(-1, ::unlink("/tmp/Action_test"));
}

}  // namespace

int main(int argc, char **argv) {
  log_setName(::basename(argv[0]));
  log_setLevel(LogLevel::DEBUG);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
