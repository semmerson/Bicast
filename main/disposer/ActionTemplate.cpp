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
    std::vector<String> argTemplate; ///< Arguments template
    const size_t        nargs;       ///< Number of command-line arguments
    const bool          persist;     ///< Should the reified action persist between products?

public:
    /**
     * Constructs.
     * @param[in] argTemplate   Argument template
     * @param[in] persist       Should this entry persist?
     */
    Impl(
            const std::vector<String> argTemplate,
            const bool                persist)
        : argTemplate(argTemplate)
        , nargs(argTemplate.size())
        , persist(persist)
    {}

    virtual ~Impl() {
    }

    /**
     * Returns the type of action.
     */
    virtual Type getType() const noexcept =0;

    /**
     * Returns the action arguments template.
     * @return The action arguments template
     */
    const std::vector<String>& getArgs() const noexcept {
        return argTemplate;
    }

    /**
     * Returns whether or not this instance should keep its file-descriptor open.
     * @retval true   Yes
     * @retval false  No
     */
    bool getKeepOpen() const noexcept {
        return persist;
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

ActionTemplate::Type ActionTemplate::getType() const noexcept {
    return pImpl->getType();
}

const std::vector<String>& ActionTemplate::getArgs() const noexcept {
    return pImpl->getArgs();
}

bool ActionTemplate::getKeepOpen() const noexcept {
    return pImpl->getKeepOpen();
}

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
     * @param[in] cmdTemplate   Command template
     * @param[in] keepOpen      Should this action be persistent (i.e., keep the pipe open)?
     */
    PipeTemplateImpl(
            const std::vector<String>& cmdTemplate,
            const bool                 keepOpen)
        : Impl{cmdTemplate, keepOpen}
    {
        if (nargs < 1)
            throw INVALID_ARGUMENT("No decoder specified");
    }

    ~PipeTemplateImpl()
    {}

    /**
     * Returns the type of action.
     * @return  The type of action
     */
    ActionTemplate::Type getType() const noexcept override {
        return ActionTemplate::Type::PIPE;
    }

    /**
     * Returns the reified action.
     * @param[in] matchResults  Results from matching the product's name
     * @return                  The reified action
     */
    Action reify(std::smatch& matchResults) override {
        std::vector<String> args(nargs);
        for (auto i = 0; i < nargs; ++i)
            args[i] = matchResults.format(argTemplate[i]);
        return PipeAction{args, persist};
    }
};

PipeTemplate::PipeTemplate(
            const std::vector<String>& cmdTemplate,
            const bool                 keepOpen)
    : ActionTemplate{new PipeTemplateImpl(cmdTemplate, keepOpen)}
{}

/******************************************************************************/

/// Action template for filing data products.
class FileTemplateImpl final : public ActionTemplate::Impl
{
public:
    /**
     * Constructs.
     * @param[in] pathTemplate  Pathname template
     * @param[in] keepOpen      Should this entry persist (i.e., keep the file open)?
     */
    FileTemplateImpl(
            const String& pathTemplate,
            const bool    keepOpen)
        : Impl{std::vector<String>{pathTemplate}, keepOpen}
    {
        if (nargs != 1)
            throw INVALID_ARGUMENT("Single pathname wasn't specified");
    }

    ~FileTemplateImpl()
    {}

    /**
     * Returns the type of action.
     * @return  The type of action
     */
    ActionTemplate::Type getType() const noexcept override {
        return ActionTemplate::Type::FILE;
    }

    /**
     * Returns a reified action.
     * @param[in] matchResults  File pathname from matching the product's name
     * @return                  A reified action
     */
    Action reify(std::smatch& matchResults) override {
        std::vector<String> args(nargs);
        for (auto i = 0; i < nargs; ++i)
            args[i] = matchResults.format(argTemplate[i]);
        return FileAction{args, persist};
    }
};

FileTemplate::FileTemplate(
            const String& pathTemplate,
            const bool    keepOpen)
    : ActionTemplate{new FileTemplateImpl(pathTemplate, keepOpen)}
{}

/******************************************************************************/

/// An implementation of a template for the action of appending data products to a file
class AppendTemplateImpl final : public ActionTemplate::Impl
{
public:
    /**
     * Constructs.
     * @param[in] pathTemplate  Pathname template
     * @param[in] keepOpen      Should the file be kept open?
     */
    AppendTemplateImpl(
            const String& pathTemplate,
            const bool    keepOpen)
        : Impl{std::vector<String>{pathTemplate}, keepOpen}
    {
        if (nargs != 1)
            throw INVALID_ARGUMENT("Single pathname wasn't specified");
    }

    ~AppendTemplateImpl()
    {}

    /**
     * Returns the type of action.
     * @return  The type of action
     */
    ActionTemplate::Type getType() const noexcept override {
        return ActionTemplate::Type::APPEND;
    }

    /**
     * Returns a reified action.
     * @param[in] matchResults  Results from matching the product's name
     * @return                  A reified action
     */
    Action reify(std::smatch& matchResults) override {
        std::vector<String> args(nargs);
        for (auto i = 0; i < nargs; ++i)
            args[i] = matchResults.format(argTemplate[i]);
        return AppendAction{args, persist};
    }
};

AppendTemplate::AppendTemplate(
            const String& pathTemplate,
            const bool    keepOpen)
    : ActionTemplate{new AppendTemplateImpl(pathTemplate, keepOpen)}
{}

} // namespace
