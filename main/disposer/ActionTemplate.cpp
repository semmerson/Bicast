/**
 * This file defines a handle class for action templates. An action template converts a template
 * command line into a concrete one based on a set of regular expression substitutions.
 *
 *  @file:  ActionTemplate.cpp
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

/**
 * A template for an action. Such a template is reified into an action by replacing its back-
 * references with matches from the data product's name.
 */
class ActionTemplate::Impl
{
protected:
    std::vector<String> argTemplates; ///< Command-line argument templates
    const size_t        nargs;        ///< Number of command-line arguments
    const bool          persist;      ///< Should the reified action persist between products?

public:
    /**
     * Constructs.
     * @param[in] argTemplates  Program argument templates
     * @param[in] persist       Should this entry persist?
     */
    Impl(
            const std::vector<String> argTemplates,
            const bool                persist)
        : argTemplates(argTemplates)
        , nargs(argTemplates.size())
        , persist(persist)
    {}

    virtual ~Impl() {
    }

    /**
     * Returns a reified action.
     * @param[in] match  Results of matching the product's name
     * @return           A reified action
     */
    virtual Action reify(std::smatch& match) =0;
};

ActionTemplate::ActionTemplate(Impl* const impl)
    : pImpl{impl}
{}

Action ActionTemplate::reify(std::smatch& match) {
    return pImpl->reify(match);
}

/******************************************************************************/

/// An implementation of a template for the action of piping data products to a program
class PipeTemplateImpl final : public ActionTemplate::Impl
{
public:
    /**
     * Constructs.
     * @param[in] argTemplates  Program argument templates
     * @param[in] keepOpen      Should this action be persistent (i.e., keep the pipe open)?
     */
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

    /**
     * Returns the reified action.
     * @param[in] matchResults  Results from matching the product's name
     * @return                  The reified action
     */
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

/// Action template for filing data products.
class FileTemplateImpl final : public ActionTemplate::Impl
{
public:
    /**
     * Constructs.
     * @param[in] argTemplates  File pathname template
     * @param keepOpen          Should this entry persist (i.e., keep the file open)?
     */
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

    /**
     * Returns a reified action.
     * @param[in] matchResults  File pathname from matching the product's name
     * @return                  A reified action
     */
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

/// An implementation of a template for the action of appending data products to a file
class AppendTemplateImpl final : public ActionTemplate::Impl
{
public:
    /**
     * Constructs.
     * @param[in] argTemplates  File pathname template
     * @param[in] keepOpen      Should the file be kept open?
     */
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

    /**
     * Returns a reified action.
     * @param[in] matchResults  Results from matching the product's name
     * @return                  A reified action
     */
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
