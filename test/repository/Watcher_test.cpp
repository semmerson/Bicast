/**
 * This file tests class `Watcher`.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Watcher_test.cpp
 * Created On: May 5, 2020
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include <fcntl.h>
#include <limits.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <Watcher.h>

namespace {

/// The fixture for testing class `Watcher`
class WatcherTest : public ::testing::Test
{
protected:
    std::string     rootDir;
    std::string     filePath;
    std::string     linkPath;
    std::string     subDirPath;
    std::string     subFilePath;

    // You can remove any or all of the following functions if its body
    // is empty.

    WatcherTest()
        : rootDir("/tmp/Watcher_test")
        , filePath(rootDir + "/" + "file")
        , linkPath()
        , subDirPath(rootDir + "/subDir")
        , subFilePath(subDirPath + "/" + "subFile")
    {
        char buf[PATH_MAX];
        linkPath = std::string(getcwd(buf, sizeof(buf))) + "/Watcher_test";
    }

    virtual ~WatcherTest()
    {
        int status;

        status = ::unlink(subFilePath.data());
        EXPECT_TRUE(status == 0 || errno == ENOENT);

        status = ::rmdir(subDirPath.data());
        EXPECT_TRUE(status == 0 || errno == ENOENT);

        status = ::unlink(filePath.data());
        EXPECT_TRUE(status == 0 || errno == ENOENT);

        EXPECT_EQ(0, ::rmdir(rootDir.data()));
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        ASSERT_EQ(0, ::mkdir(rootDir.data(), 0777));
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.
};

// Tests default construction
TEST_F(WatcherTest, DefaultConstruction)
{
    hycast::Watcher watcher(rootDir);
}

// Tests adding a file
TEST_F(WatcherTest, AddFile)
{
    hycast::Watcher watcher(rootDir);

    int             fd = ::open(filePath.data(), O_WRONLY|O_CREAT|O_EXCL, 0600);
    ASSERT_NE(-1, fd);
    ::close(fd);

    struct hycast::Watcher::WatchEvent watchEvent;
    watcher.getEvent(watchEvent);
    ASSERT_EQ(filePath, watchEvent.pathname);
    LOG_NOTE("Event: \"%s\"", filePath.data());
}

// Tests adding a symbolic link
TEST_F(WatcherTest, AddSymbolicLink)
{
    hycast::Watcher watcher(rootDir);

    ASSERT_NE(-1, symlink(linkPath.data(), filePath.data()));

    struct hycast::Watcher::WatchEvent watchEvent;
    watcher.getEvent(watchEvent);
    ASSERT_EQ(filePath, watchEvent.pathname);
    LOG_NOTE("Event: \"%s\"", filePath.data());
}

// Tests adding a hard link
TEST_F(WatcherTest, AddHardLink)
{
    hycast::Watcher watcher(rootDir);

    ASSERT_NE(-1, link(linkPath.data(), filePath.data()));

    struct hycast::Watcher::WatchEvent watchEvent;
    watcher.getEvent(watchEvent);
    ASSERT_EQ(filePath, watchEvent.pathname);
    LOG_NOTE("Event: \"%s\"", filePath.data());
}

// Tests adding a subdirectory and a file
TEST_F(WatcherTest, AddSubDirFile)
{
    hycast::Watcher watcher(rootDir);

    int status = ::mkdir(subDirPath.data(), 0777);
    ASSERT_EQ(0, status);

    int fd = ::open(subFilePath.data(), O_WRONLY|O_CREAT|O_EXCL, 0666);
    ASSERT_NE(-1, fd);
    ::close(fd);

    struct hycast::Watcher::WatchEvent watchEvent;
    watcher.getEvent(watchEvent);
    ASSERT_EQ(subFilePath, watchEvent.pathname);
    LOG_NOTE("Event: \"%s\"", subFilePath.data());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
