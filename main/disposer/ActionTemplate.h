/**
 * This file declares a handle class for action templates.
 *
 *  @file:  ActionTemplate.h
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

#ifndef MAIN_DISPOSER_ACTIONTEMPLATE_H_
#define MAIN_DISPOSER_ACTIONTEMPLATE_H_

#include "Action.h"
#include "CommonTypes.h"

#include <memory>
#include <regex>
#include <vector>

namespace hycast {

class ActionTemplate
{
public:
    class                 Impl;

private:
    std::shared_ptr<Impl> pImpl;

protected:
    ActionTemplate(Impl* const impl);

public:
    /**
     * Default constructs. Will test false.
     */
    ActionTemplate() =default;

    /**
     * Constructs.
     *
     * @param[in] argTemplates  Command-line argument templates
     */
    ActionTemplate(const std::vector<String>& argTemplates);

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval `true`  This instance is valid
     * @retval `false` This instance is not valid
     */
    operator bool() const noexcept;

    /**
     * Returns a concrete action resulting from substituting values in the argument templates.
     *
     * @param[in] match  Results of regular expression matching
     * @return           Corresponding concrete action
     */
    Action reify(std::smatch& match);
};

/**
 * An action template for the pipe action.
 */
class PipeTemplate final : public ActionTemplate
{
public:
    /**
     * Constructs.
     *
     * @param[in] argTemplates  Command-line argument templates
     * @param[in] keepOpen      Should the pipe to the decoder be kept open between products?
     */
    PipeTemplate(
            const std::vector<String>& argTemplates,
            const bool                 keepOpen);
};

/**
 * An action template for the file action.
 */
class FileTemplate final : public ActionTemplate
{
public:
    /**
     * Constructs.
     *
     * @param[in] argTemplates  Command-line argument templates
     * @param[in] keepOpen      Should the file be kept open between products?
     */
    FileTemplate(
            const std::vector<String>& argTemplates,
            const bool                 keepOpen);
};

/**
 * An action template for the append action.
 */
class AppendTemplate final : public ActionTemplate
{
public:
    /**
     * Constructs.
     *
     * @param[in] argTemplates  Command-line argument templates
     * @param[in] keepOpen      Should the file be kept open between products?
     */
    AppendTemplate(
            const std::vector<String>& argTemplates,
            const bool                 keepOpen);
};

} // namespace

#endif /* MAIN_DISPOSER_ACTIONTEMPLATE_H_ */
