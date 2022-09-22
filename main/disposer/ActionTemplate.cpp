/**
 * This file defines a handle class for action templates. An action template converts a template
 * command line into a concrete one based on a set of regular expression substitutions.
 *
 *  @file:  ActionTemplate
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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
 */

#include "config.h"

#include "ActionTemplate.h"

namespace hycast {

class ActionTemplate::Impl
{
protected:
    std::vector<String> argTemplates; ///< Command-line argument templates
    const size_t        nargs;        ///< Number of command-line arguments
    const bool          persist;      ///< Should the reified action persist between products?

public:
    Impl(
            const std::vector<String> argTemplates,
            const bool                persist)
        : argTemplates(argTemplates)
        , nargs(argTemplates.size())
        , persist(persist)
    {}

    virtual ~Impl() {
    }

    virtual Action reify(std::smatch& match) =0;
};

ActionTemplate::ActionTemplate(Impl* const impl)
    : pImpl{impl}
{}

Action ActionTemplate::reify(std::smatch& match) {
    return pImpl->reify(match);
}

/******************************************************************************/

class PipeTemplateImpl final : public ActionTemplate::Impl
{
public:
    PipeTemplateImpl(
            const std::vector<String>& argTemplates,
            const bool                 keepOpen)
        : Impl{argTemplates, keepOpen}
    {
        if (argTemplates.size() < 1)
            throw INVALID_ARGUMENT("No decoder arguments");
    }

    ~PipeTemplateImpl()
    {}

    Action reify(std::smatch& matchResults) override {
        std::vector<String> args(nargs);
        for (auto i = 0; i < nargs; ++i)
            args[i] = matchResults.format(argTemplates[i]);
        return PipeAction{args, persist};
    }
};

PipeTemplate::PipeTemplate(
            const std::vector<String>& argTemplates,
            const bool                 keepOpen)
    : ActionTemplate{new PipeTemplateImpl(argTemplates, keepOpen)}
{}

/******************************************************************************/

class FileTemplateImpl final : public ActionTemplate::Impl
{
public:
    FileTemplateImpl(
            const std::vector<String>& argTemplates,
            const bool                 keepOpen)
        : Impl{argTemplates, keepOpen}
    {
        if (argTemplates.size() < 1)
            throw INVALID_ARGUMENT("No decoder arguments");
    }

    ~FileTemplateImpl()
    {}

    Action reify(std::smatch& matchResults) override {
        std::vector<String> args(nargs);
        for (auto i = 0; i < nargs; ++i)
            args[i] = matchResults.format(argTemplates[i]);
        return FileAction{args, persist};
    }
};

FileTemplate::FileTemplate(
            const std::vector<String>& argTemplates,
            const bool                 keepOpen)
    : ActionTemplate{new FileTemplateImpl(argTemplates, keepOpen)}
{}

/******************************************************************************/

class AppendTemplateImpl final : public ActionTemplate::Impl
{
public:
    AppendTemplateImpl(
            const std::vector<String>& argTemplates,
            const bool                 keepOpen)
        : Impl{argTemplates, keepOpen}
    {
        if (argTemplates.size() < 1)
            throw INVALID_ARGUMENT("No decoder arguments");
    }

    ~AppendTemplateImpl()
    {}

    Action reify(std::smatch& matchResults) override {
        std::vector<String> args(nargs);
        for (auto i = 0; i < nargs; ++i)
            args[i] = matchResults.format(argTemplates[i]);
        return AppendAction{args, persist};
    }
};

AppendTemplate::AppendTemplate(
            const std::vector<String>& argTemplates,
            const bool                 keepOpen)
    : ActionTemplate{new AppendTemplateImpl(argTemplates, keepOpen)}
{}

} // namespace
