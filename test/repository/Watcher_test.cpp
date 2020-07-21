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

#include <Watcher.h>

#include "error.h"
#include "FileUtil.h"

#include <fcntl.h>
#include <limits.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <thread>

namespace {

/// The fixture for testing class `Watcher`
class WatcherTest : public ::testing::Test
{
protected:
    std::string     testDir;
    std::string     rootDir;
    std::string     filename;
    std::string     relFilePath;

    // You can remove any or all of the following functions if its body
    // is empty.

    WatcherTest()
        : testDir("/tmp/Watcher_test")
        , rootDir(testDir + "/watched")
        , filename("file.dat")
        , relFilePath(std::string("foo/bar/") + filename)
    {
        char buf[PATH_MAX];
        hycast::rmDirTree(testDir);
        hycast::ensureDir(rootDir);
    }

    virtual ~WatcherTest()
    {
        hycast::rmDirTree(rootDir);
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {}

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.

    void createFile(const std::string pathname) {
        hycast::ensureParent(pathname);
        const int fd = ::open(pathname.data(), O_WRONLY|O_CREAT|O_EXCL, 0600);
        ASSERT_NE(-1, fd);
        ::close(fd);
    }

public:
    void getNextFile(
            hycast::Watcher*   watcher,
            const std::string* pathname) {
        struct hycast::Watcher::WatchEvent watchEvent;

        watcher->getEvent(watchEvent);
        ASSERT_EQ(*pathname, watchEvent.pathname);
        LOG_NOTE("Event: \"%s\"", pathname->data());
    }
};

// Tests default construction
TEST_F(WatcherTest, DefaultConstruction)
{
    hycast::Watcher watcher(rootDir);
}

// Tests adding a file
TEST_F(WatcherTest, AddFile)
{
    hycast::Watcher   watcher(rootDir);
    const std::string filePath = rootDir + "/" + relFilePath;
    auto              thrd = std::thread(&WatcherTest::getNextFile, this,
            &watcher, &filePath);

    createFile(filePath);

    thrd.join();
}

// Tests adding a symbolic link
TEST_F(WatcherTest, AddSymbolicLink)
{
    hycast::Watcher   watcher(rootDir);
    const std::string filePath = testDir + "/" + relFilePath;
    const std::string linkPath = rootDir + "/" + relFilePath;
    auto              thread = std::thread(&WatcherTest::getNextFile, this,
            &watcher, &linkPath);

    createFile(filePath);
    hycast::ensureParent(linkPath);
    ASSERT_NE(-1, ::symlink(filePath.data(), linkPath.data()));

    thread.join();
}

// Tests adding a hard link
TEST_F(WatcherTest, AddHardLink)
{
    hycast::Watcher   watcher(rootDir);
    const std::string filePath = testDir + "/" + relFilePath;
    const std::string linkPath = rootDir + "/" + relFilePath;
    auto              thread = std::thread(&WatcherTest::getNextFile, this,
            &watcher, &linkPath);

    createFile(filePath);
    hycast::ensureParent(linkPath);
    ASSERT_NE(-1, ::link(filePath.data(), linkPath.data()));

    thread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
