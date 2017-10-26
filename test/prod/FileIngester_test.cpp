/**
 * This file tests class `FileIngester`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: FileIngester_test.cpp
 * Created On: Oct 24, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "FileIngester.h"

#include <cstdio>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <string>
#include <sys/stat.h>

namespace {

/// The fixture for testing class `FileIngester`
class FileIngesterTest : public ::testing::Test
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    FileIngesterTest()
    {
        std::remove(rootDirPathname.data());
        ::mkdir(rootDirPathname.data(), S_IRWXU);
    }

    virtual ~FileIngesterTest()
    {
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
        std::remove(rootDirPathname.data());
    }

    // Objects declared here can be used by all tests in the test case for Error.
    const std::string rootDirPathname{"/tmp/FileIngester"};
};

// Tests default construction
TEST_F(FileIngesterTest, DefaultConstruction)
{
    hycast::FileIngester ingester{};
    EXPECT_FALSE(ingester);
}

// Tests invalid directory construction
TEST_F(FileIngesterTest, InvalidDirectoryConstruction)
{
    EXPECT_THROW(hycast::FileIngester{"/bad/directory"}, hycast::SystemError);
    EXPECT_THROW(hycast::FileIngester{"/dev/null"}, hycast::SystemError);
}

#if 0
// Tests empty directory construction
TEST_F(FileIngesterTest, EmptyDirectoryConstruction)
{
    hycast::FileIngester ingester{rootDirPathname};
    auto prod = ingester.getProduct();
    EXPECT_FALSE(prod);
}

// Tests non-empty directory construction
TEST_F(FileIngesterTest, NonEmptyDirectoryConstruction)
{
    hycast::FileIngester ingester{rootDirPathname};
    const auto pathname = rootDirPathname + "/" + "foo.bar";
    auto fd = ::open(pathname.data(), O_WRONLY|O_CREAT);
    ASSERT_NE(-1, fd);
    ::close(fd);
    auto prod = ingester.getProduct();
    ASSERT_TRUE(prod);
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
