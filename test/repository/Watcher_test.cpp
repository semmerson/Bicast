/**
 * This file tests class `Watcher`.
 *
 *       File: Watcher_test.cpp
 * Created On: May 5, 2020
 *     Author: Steven R. Emmerson
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
#include "config.h"

#include "Watcher.h"

#include "error.h"
#include "FileUtil.h"

#include <dirent.h>
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
        hycast::FileUtil::rmDirTree(testDir);
        hycast::FileUtil::ensureDir(rootDir);
    }

    virtual ~WatcherTest()
    {
        hycast::FileUtil::rmDirTree(rootDir);
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
        hycast::FileUtil::ensureParent(pathname);
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
        LOG_DEBUG("Event: \"%s\"", pathname->data());
    }
};

// Tests default construction
TEST_F(WatcherTest, DefaultConstruction)
{
    hycast::Watcher watcher(rootDir);
}

// Tests making and deleting a directory
TEST_F(WatcherTest, DeleteDirectory)
{
    hycast::Watcher   watcher(rootDir);
    const std::string dirPath = rootDir + "/subdir";

    ASSERT_EQ(0, ::mkdir(dirPath.data(), 0777));
    DIR* const dirStream = ::opendir(dirPath.data());
    ASSERT_NE(nullptr, dirStream);
    ASSERT_EQ(0, ::closedir(dirStream));

    ASSERT_EQ(0, ::rmdir(dirPath.data()));
    ASSERT_EQ(nullptr, ::opendir(dirPath.data()));
}

// Tests adding a file
TEST_F(WatcherTest, AddFile)
{
    hycast::Watcher   watcher(rootDir);
    const std::string filePath = rootDir + "/" + relFilePath;
    auto              thrd = std::thread(&WatcherTest::getNextFile, this, &watcher, &filePath);

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
    hycast::FileUtil::ensureParent(linkPath);
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
    hycast::FileUtil::ensureParent(linkPath);
    ASSERT_NE(-1, ::link(filePath.data(), linkPath.data()));

    thread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
