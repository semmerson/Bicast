/**
 * This file contains utility functions for parsing.
 *
 *  @file:  Parser.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef MAIN_MISC_PARSER_H_
#define MAIN_MISC_PARSER_H_

#include "CommonTypes.h"
#include "error.h"

#include <yaml-cpp/yaml.h>

namespace hycast {

/// Class for parsing a scalar YAML value.
class Parser
{
public:
    /**
     * Tries to decode a scalar (i.e., primitive) value in a YAML map. Does nothing if the scalar
     * doesn't exist.
     *
     * @tparam     T                 Type of scalar value
     * @param[in]  parent            Map containing the scalar
     * @param[in]  key               Name of the scalar
     * @param[out] value             Scalar value
     * @throw std::invalid_argument  Parent node isn't a map
     * @throw std::invalid_argument  Subnode with given name isn't a scalar
     */
    template<class T>
    static void tryDecode(YAML::Node&   parent,
                          const String& key,
                          T&            value)
    {
        if (!parent.IsMap())
            throw INVALID_ARGUMENT("Node \"" + parent.Tag() + "\" isn't a map");

        auto child = parent[key];

        if (child) {
            if (!child.IsScalar())
                throw INVALID_ARGUMENT("Node \"" + key + "\" isn't scalar");

            value = child.as<T>();
        }
    }
};

} // namespace

#endif /* MAIN_MISC_PARSER_H_ */
