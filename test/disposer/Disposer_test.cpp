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
#include <fstream>
#include <iostream>
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
        : rootDir("/tmp/Disposer_test/")
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

// Tests filing
TEST_F(DisposerTest, Filing)
{
    try {
        struct Entry {
            const char* prodName;
            const char* pattern;
            const char* filePat;
            const char* pathname;
            Entry(const char* prodName, const char* pattern, const char* filePat,
                    const char* pathname)
                : prodName(prodName)
                , pattern(pattern)
                , filePat(filePat)
                , pathname(pathname)
            {}
        };
        Entry entries[] = {
                // gcc 4.8.5 std::regex doesn't support brackets!!! 4.9 does.
                Entry{"prod1",         "prod1",               "$&",          "prod1"},
                Entry{"foo/prod2",     "\\w+/prod2$",         "$&",          "foo/prod2"},
                Entry{"foo/bar/prod3", "((\\w+/){2})(prod3)", "$1/$3",       "foo/bar/prod3"},
                Entry{"bar/prod4/foo", "(\\w+)/prod4/(\\w+)", "$2/$1/prod4", "foo/bar/prod4"},
        };
        Pattern  excl{};  // Exclude nothing
        Disposer disposer(0);

        for (auto& entry : entries) {
            Pattern       incl(entry.pattern);
            String        pathTemplate(rootDir + entry.filePat);
            FileTemplate  fileTemplate(pathTemplate, true);
            PatternAction patAct(incl, excl, fileTemplate);

            disposer.add(patAct);
        }

        for (ProdSize i = 0; i < sizeof(entries)/sizeof(Entry); ++i) {
            String contents(std::to_string(i));
            ProdInfo prodInfo(entries[i].prodName, contents.size());
            disposer.dispose(prodInfo, contents.data());
        }

        for (ProdSize i = 0; i < sizeof(entries)/sizeof(Entry); ++i) {
            std::ifstream input(rootDir+entries[i].pathname);
            char contents[80];
            input >> contents;
            EXPECT_STREQ(contents, std::to_string(i).data());
        }
    }
    catch (const std::regex_error& e) {
        std::cout << "regex_error caught: " << e.what() << '\n';
        if (e.code() == std::regex_constants::error_brack)
            std::cout << "The code was error_brack\n";
        throw;
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        throw;
    }
}

// Tests appending
TEST_F(DisposerTest, Appending)
{
    try {
        Pattern    excl{};      // Exclude nothing
        Disposer   disposer(0); // No persistent actions

        Pattern        incl("prod");
        String         pathTemplate(rootDir + "$&");
        AppendTemplate appendTemplate(pathTemplate, true);
        PatternAction  patAct(incl, excl, appendTemplate);
        disposer.add(patAct);

        String contents("1");
        ProdInfo prodInfo("prod", contents.size());
        disposer.dispose(prodInfo, contents.data());

        contents = String("2");
        disposer.dispose(prodInfo, contents.data());

        std::ifstream input(rootDir+"prod");
        char buf[80];
        input >> buf;
        EXPECT_STREQ(buf, "12");
    }
    catch (const std::regex_error& e) {
        std::cout << "regex_error caught: " << e.what() << '\n';
        if (e.code() == std::regex_constants::error_brack)
            std::cout << "The code was error_brack\n";
        throw;
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        throw;
    }
}

// Tests piping
TEST_F(DisposerTest, Piping)
{
    try {
        Pattern    excl{};      // Exclude nothing
        Disposer   disposer(0); // No persistent actions

        Pattern             incl("prod");
        std::vector<String> cmdTemplate{"sh", "-c", String("cat >") + rootDir + "$&"};
        PipeTemplate        pipeTemplate(cmdTemplate, true);
        PatternAction       patAct(incl, excl, pipeTemplate);
        disposer.add(patAct);

        String contents("1");
        ProdInfo prodInfo("prod", contents.size());
        disposer.dispose(prodInfo, contents.data());

        std::ifstream input(rootDir+"prod");
        char buf[80];
        input >> buf;
        EXPECT_STREQ(buf, "1");
    }
    catch (const std::regex_error& e) {
        std::cout << "regex_error caught: " << e.what() << '\n';
        if (e.code() == std::regex_constants::error_brack)
            std::cout << "The code was error_brack\n";
        throw;
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        throw;
    }
}

// Tests excluding
TEST_F(DisposerTest, Excluding)
{
    try {
        Disposer   disposer(0); // No persistent actions

        Pattern        incl("prod");
        Pattern        excl{"prod"};
        String         pathTemplate(rootDir + "$&");
        FileTemplate   fileTemplate(pathTemplate, true);
        PatternAction  patAct(incl, excl, fileTemplate);
        disposer.add(patAct);

        String   contents("1");
        ProdInfo prodInfo("prod", contents.size());
        disposer.dispose(prodInfo, contents.data());

        std::ifstream input(rootDir+"prod");
        EXPECT_EQ(input.rdstate(), std::ios_base::failbit);
    }
    catch (const std::regex_error& e) {
        std::cout << "regex_error caught: " << e.what() << '\n';
        if (e.code() == std::regex_constants::error_brack)
            std::cout << "The code was error_brack\n";
        throw;
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        throw;
    }
}

// Tests YAML config-file
#if 1
TEST_F(DisposerTest, ConfigFile)
{
    try {
        auto disposer = Disposer::createFromYaml(configFile);
        //std::cout << disposer.getYaml();
        const String expect(
            "MaxKeepOpen: 20\n"
            "PatternActions:\n"
            "  - Include: ^SAUS(..) (....)\n"
            "    Pipe: [sh, -c, cat >>surface/US/$2/$1]\n"
            "    KeepOpen: true\n"
            "  - Include: ^WS\n"
            "    File: WWA/lastSIGMET");
        EXPECT_STREQ(Disposer::getYaml(disposer).data(), expect.data());
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex);
        throw;
    }
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  log_setName(FileUtil::filename(argv[0]));
  log_setLevel(LogLevel::INFO);
  std::cout << "argc=" << argc <<'\n';
  std::cout << "argv[0]=" << std::string(argv[0]) << '\n';
  std::cout << "argv[1]=" << std::string(argv[1]) << '\n';
  configFile = std::string(argv[1]);
  return RUN_ALL_TESTS();
}
