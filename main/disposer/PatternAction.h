/**
 * This file declares a pattern/action pair. The pattern comprises two regular expressions that are
 * matched against the product name. In order for a product to be processed by the action, its
 * name must match the "include" regular expressions and *not* match the "exclude" regular
 * expression.
 *
 *  @file:  PatternAction.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef MAIN_DISPOSER_PATTERNACTION_H_
#define MAIN_DISPOSER_PATTERNACTION_H_

#include "ActionTemplate.h"

#include <memory>
#include <regex>

namespace hycast {

/// A regular expression pattern
class Pattern {
    String     string;  ///< String representation of the pattern
    std::regex regex;   ///< Compiled pattern

public:
    /// Default constructs
    Pattern()
        : string()
        , regex()
    {}

    /// Constructs
    Pattern(const String& string)
        : string(string)
        , regex(string)
    {}

    /// Returns the string representation.
    const String& to_string() const {
        return string;
    }

    /// Returns the compiled pattern
    const std::regex& getRegex() const {
        return regex;
    }
};

/// A pattern & action
struct PatternAction
{
    Pattern        include;        ///< Product names to be included
    Pattern        exclude;        ///< Product names to be excluded
    ActionTemplate actionTemplate; ///< Command-line template to be reified

    PatternAction()
        : include()         // Matches nothing
        , exclude()         // Matches nothing
        , actionTemplate()  // Will test false
    {}

    /**
     * Constructs.
     * @param[in] include         The pattern a product's name must match
     * @param[in] exclude         The pattern a product's name must not match
     * @param[in] actionTemplate  The template for acting on matching products
     */
    PatternAction(
            const Pattern&        include,
            const Pattern&        exclude,
            const ActionTemplate& actionTemplate)
        : include(include)
        , exclude(exclude)
        , actionTemplate(actionTemplate)
    {}
};

} // namespace

#endif /* MAIN_DISPOSER_PATTERNACTION_H_ */
