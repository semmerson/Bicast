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

#include "CommonTypes.h"
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

using namespace bicast;

/// The fixture for testing class `Watcher`
class WatcherTest : public ::testing::Test, public Watcher::Client
{
protected:
    std::string     testDir;
    std::string     rootDir;
    std::string     relDirPath;
    std::string     filename;
    std::string     relFilePath;
    String          expectedPathname;

    // You can remove any or all of the following functions if its body
    // is empty.

    WatcherTest()
        : testDir("/tmp/Watcher_test")
        , rootDir(testDir + "/watched")
        , relDirPath("foo/bar")
        , filename("file.dat")
        , relFilePath(relDirPath + "/" + filename)
        , expectedPathname()
    {
        char buf[PATH_MAX];
        FileUtil::rmDirTree(testDir);
        FileUtil::ensureDir(rootDir);
    }

    virtual ~WatcherTest()
    {
        FileUtil::rmDirTree(rootDir);
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
        FileUtil::ensureParent(pathname);
        const int fd = ::open(pathname.data(), O_WRONLY|O_CREAT|O_EXCL, 0600);
        ASSERT_NE(-1, fd);
        ::close(fd);
    }

public:
    /**
     * Processes the addition of a directory.
     * @param[in] pathname  Absolute pathname of the directory
     */
    void dirAdded(const String& pathname) override {
    }

    /**
     * Processes the addition of a complete, regular file.
     * @param[in] pathname  Absolute pathname of the complete, regular file
     */
    void fileAdded(const String& pathname) override {
        ASSERT_EQ(expectedPathname, pathname);
    }
};

// Tests default construction
TEST_F(WatcherTest, DefaultConstruction)
{
    Watcher watcher(rootDir, *this);
}

// Tests making and deleting a directory
TEST_F(WatcherTest, DeleteDirectory)
{
    Watcher   watcher(rootDir, *this);
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
    Watcher   watcher(rootDir, *this);

    expectedPathname = rootDir + "/" + relFilePath;
    createFile(expectedPathname);
}

// Tests adding a symbolic link to a file
TEST_F(WatcherTest, AddFileSymLink)
{
    Watcher   watcher(rootDir, *this);
    const std::string filePath = testDir + "/" + relFilePath;

    expectedPathname = rootDir + "/" + relFilePath;
    createFile(filePath);
    FileUtil::ensureParent(expectedPathname);
    ASSERT_NE(-1, ::symlink(filePath.data(), expectedPathname.data()));
}

// Tests adding a symbolic link to a directory
TEST_F(WatcherTest, AddDirectorySymLink)
{
    Watcher   watcher(rootDir, *this);
    const std::string dirPath = testDir + "/" + relDirPath;
    const std::string filePath = dirPath + "/" + filename;
    const std::string dirLinkPath = rootDir + "/" + relDirPath;
    expectedPathname = dirLinkPath + "/" + filename;

    createFile(filePath);
    FileUtil::ensureParent(dirLinkPath);
    ASSERT_NE(-1, ::symlink(dirPath.data(), dirLinkPath.data()));
}

// Tests adding a hard link to a file
TEST_F(WatcherTest, AddFileHardLink)
{
    Watcher   watcher(rootDir, *this);
    const std::string filePath = testDir + "/" + relFilePath;
    expectedPathname = rootDir + "/" + relFilePath;

    createFile(filePath);
    FileUtil::ensureParent(expectedPathname);
    ASSERT_NE(-1, ::link(filePath.data(), expectedPathname.data()));
}

#if 0
// A hard link to a directory doesn't work!

// Tests adding a hard link to a directory
TEST_F(WatcherTest, AddHardLink)
{
    Watcher   watcher(rootDir, 60);
    const std::string filePath = testDir + "/" + relFilePath;
    const std::string linkPath = rootDir + "/" + relFilePath;
    auto              thread = std::thread(&WatcherTest::getNextFile, this,
            &watcher, &linkPath);

    createFile(filePath);
    FileUtil::ensureParent(linkPath);
    ASSERT_NE(-1, ::link(filePath.data(), linkPath.data()));

    thread.join();
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
