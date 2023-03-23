/**
 * This file tests class `Disposer`.
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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
 *
 *       File: disposer_test.cpp
 * Created On: 2022-09-01T12:50:29-0600
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "Disposer.h"
#include "FileUtil.h"
#include "logging.h"

#include <gtest/gtest.h>
#include <unistd.h>

static std::string configFile;

using namespace hycast;

namespace {

/// The fixture for testing class `Disposer`
class DisposerTest : public ::testing::Test
{
protected:
    String rootDir;

    // You can remove any or all of the following functions if its body
    // is empty.

    DisposerTest()
        : rootDir("/tmp/Disposer_test")
    {
        FileUtil::rmDirTree(rootDir);
        FileUtil::ensureDir(rootDir);
    }

    virtual ~DisposerTest() {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown() {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }
};

// Tests construction
TEST_F(DisposerTest, Construction)
{
    Disposer disposer(0);
}

// Tests simple usage
TEST_F(DisposerTest, SimpleUsage)
{
    try {
        Disposer            disposer(0);
        std::regex          incl("(one|two)"); // gcc 4.8.5 doesn't support brackets!!! 4.9 does.
        std::regex          excl("two");
        std::vector<String> cmdTemplate{"sh", "-c", "cat >" + rootDir + "/$1"};
        // `true` to see how this works with no persistent actions allowed
        PipeTemplate        actionTemplate(cmdTemplate, true);
        PatternAction       patAct(incl, excl, actionTemplate);

        disposer.add(patAct);

        ProdInfo prodInfo1("one", 1);
        disposer.dispose(prodInfo1, "1");

        ProdInfo prodInfo2("two", 1);
        disposer.dispose(prodInfo2, "2");
    }
    catch (const std::regex_error& e) {
        std::cout << "regex_error caught: " << e.what() << '\n';
        if (e.code() == std::regex_constants::error_brack)
            std::cout << "The code was error_brack\n";
    }
}

// Tests YAML config-file
#if 0
TEST_F(DisposerTest, ConfigFile)
{
    auto disposer = Disposer::create(configFile);
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  log_setName(FileUtil::filename(argv[0]));
  log_setLevel(LogLevel::DEBUG);
  std::cout << "argc=" << argc <<'\n';
  std::cout << "argv[0]=" << std::string(argv[0]) << '\n';
  //std::cout << "argv[1]=" << std::string(argv[1]) << '\n';
  configFile = std::string(argv[1]);
  return RUN_ALL_TESTS();
}
