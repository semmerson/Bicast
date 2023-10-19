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

#include <regex>
#include <yaml-cpp/yaml.h>

namespace bicast {

/// Class for parsing a scalar YAML value.
class Parser
{
public:
    /**
     * Tries to decode a scalar (i.e., primitive) value in a YAML map. Does nothing if the scalar
     * doesn't exist.
     *
     * @tparam     T           Type of scalar value
     * @param[in]  mapNode     Map containing the scalar
     * @param[in]  key         Name of the scalar
     * @param[out] value       Scalar value
     * @retval     true        Scalar exists and was successfully decoded into `value`
     * @retval     false       Scalar doesn't exist. `value` is unchanged.
     * @throw InvalidArgument  Node isn't a map
     * @throw InvalidArgument  Subnode with given name isn't a scalar
     * @throw InvalidArgument  Scalar couldn't be decoded as given type
     */
    template<class T>
    static bool tryDecode(YAML::Node&   mapNode,
                          const String& key,
                          T&            value) {
        if (!mapNode.IsMap())
            throw INVALID_ARGUMENT("Node \"" + mapNode.Tag() + "\" isn't a map");

        auto child = mapNode[key];

        if (child) {
            if (!child.IsScalar())
                throw INVALID_ARGUMENT("Node \"" + key + "\" isn't scalar");

            try {
                value = child.as<T>();
            }
            catch (const std::exception& ex) {
                std::throw_with_nested(INVALID_ARGUMENT("Couldn't decode \"" + key + "\" value \"" +
                        child.as<String>() + "\""));
            }
        }

        return static_cast<bool>(child);
    }

    /**
     * Tries to decode a sequence of scalar values in a YAML map. Does nothing if the sequence
     * doesn't exist.
     *
     * @tparam     T           Type of scalar value
     * @param[in]  mapNode     Map containing the sequence
     * @param[in]  key         Name of the sequence
     * @param[out] sequence    Sequence
     * @retval     true        Sequence exists and was successfully decoded into `sequence`
     * @retval     false       Sequence doesn't exist. `sequence` is unchanged.
     * @throw InvalidArgument  Node isn't a map
     * @throw InvalidArgument  Subnode with given name isn't a sequence
     * @throw InvalidArgument  Scalar couldn't be decoded as given type
     */
    template<class T>
    static bool tryDecode(YAML::Node&     mapNode,
                          const String&   key,
                          std::vector<T>& sequence) {
        if (!mapNode.IsMap())
            throw INVALID_ARGUMENT("Node \"" + mapNode.Tag() + "\" isn't a map");

        auto child = mapNode[key];

        if (child) {
            if (!child.IsSequence())
                throw INVALID_ARGUMENT("Node \"" + key + "\" isn't a sequence");

            try {
                auto n = child.size();
                std::vector<T> seq(n);
                for (size_t i = 0; i < n; ++i)
                    seq[i] = child[i].as<T>();
                sequence = seq;
            }
            catch (const std::exception& ex) {
                std::throw_with_nested(RUNTIME_ERROR("Couldn't decode \"" + key + "\" value \"" +
                        child.as<String>() + "\""));
            }
        }

        return static_cast<bool>(child);
    }

    /**
     * Decodes an integer scalar.
     * @param[in]  mapNode     Map containing the scalar
     * @param[in]  key         Name of the scalar
     * @param[out] value       Scalar value
     * @retval     true        Scalar exists and was successfully decoded into `value`
     * @retval     false       Scalar doesn't exist. `value` is unchanged.
     * @throw InvalidArgument  Node isn't a map
     * @throw InvalidArgument  Subnode with given name isn't a scalar
     * @throw InvalidArgument  Scalar couldn't be decoded as given type
     */
    static bool tryDecode(
            YAML::Node&   mapNode,
            const String& key,
            int&          value) {
        return tryDecode<int>(mapNode, key, value);
    }

    /**
     * Decodes a string scalar.
     * @param[in]  mapNode     Map containing the scalar
     * @param[in]  key         Name of the scalar
     * @param[out] value       Scalar value
     * @retval     true        Scalar exists and was successfully decoded into `value`
     * @retval     false       Scalar doesn't exist. `value` is unchanged.
     * @throw InvalidArgument  Node isn't a map
     * @throw InvalidArgument  Subnode with given name isn't a scalar
     * @throw InvalidArgument  Scalar couldn't be decoded as given type
     */
    static bool tryDecode(
            YAML::Node&   mapNode,
            const String& key,
            String&       value) {
        return tryDecode<String>(mapNode, key, value);
    }

    /**
     * Decodes a regular expression scalar.
     * @param[in]  mapNode     Map containing the scalar
     * @param[in]  key         Name of the scalar
     * @param[out] value       Scalar value
     * @retval     true        Scalar exists and was successfully decoded into `value`
     * @retval     false       Scalar doesn't exist. `value` is unchanged.
     * @throw InvalidArgument  Node isn't a map
     * @throw InvalidArgument  Subnode with given name isn't a scalar
     * @throw InvalidArgument  Scalar value isn't a valid regular expression
     */
    static bool tryDecode(
            YAML::Node&   mapNode,
            const String& key,
            std::regex&   value) {
        String     str;
        const bool exists = tryDecode(mapNode, key, str);
        if (exists) {
            try {
                value = std::regex(str);
            }
            catch (const std::exception& ex) {
                throw INVALID_ARGUMENT("Invalid regular expression: \"" + str + "\"");
            }
        }
        return exists;
    }

    /**
     * Decodes a string sequence.
     * @param[in]  mapNode     Map containing the sequence
     * @param[in]  key         Name of the sequence
     * @param[out] sequence    Sequence
     * @retval     true        Sequence exists and was successfully decoded into `sequence`
     * @retval     false       Sequence doesn't exist. `sequence` is unchanged.
     * @throw InvalidArgument  Node isn't a map
     * @throw InvalidArgument  Subnode with given name isn't a sequence
     * @throw InvalidArgument  Scalars couldn't be decoded as strings
     */
    static bool tryDecode(
            YAML::Node&          mapNode,
            const String&        key,
            std::vector<String>& sequence) {
        return tryDecode<String>(mapNode, key, sequence);
    }
};

} // namespace

#endif /* MAIN_MISC_PARSER_H_ */
