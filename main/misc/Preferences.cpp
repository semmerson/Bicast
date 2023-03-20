/**
 * This file  
 *
 *  @file:  
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#include "error.h"
#include "Preferences.h"

#include <initializer_list>
#include <sstream>
#include <yaml-cpp/yaml.h>

namespace hycast {

using namespace std;

/// An implementation of preferences
class PrefImpl : public Preferences
{
    YAML::Node root; ///< yaml-cpp root node

    /**
     * @tparam    T               Type of preference's value
     * @param[in] def             Preference's default value
     * @param[in] names           Preference's name components from outermost to
     *                            innermost
     * @return                    Pair whose first element is value and whose
     *                            second element indicates if preference was
     *                            found
     * @throw std::runtime_error  yaml-cpp failure
     */
    template<typename T>
    pair<T, bool> getAs(
            T&                    def,
            const vector<String>& names) {
        YAML::Node node{root};
        for (auto name : names) {
            node = node[name];
            if (!node)
                return {def, false};
        }
        return {node.as<T>(), true}; // Will throw if not convertible
    }

    /**
     * Returns the string representation of a preference's name.
     *
     * @param[in names  Preference's name components from outermost to innermost
     * @return          String representation of preference's name
     */
    String makeString(const vector<String>& names) {
        ostringstream keyStrStrm;
        for (const auto& name : names)
            keyStrStrm << name << '/';
        auto keyStr = keyStrStrm.str();
        if (keyStr.length())
            keyStr.erase(keyStr.length()-1);
        return keyStr;
    }

public:
    /**
     * Constructs.
     * @param[in] pathname  Pathname of the YAML file
     */
    PrefImpl(const String& pathname)
        : root(YAML::LoadFile(pathname))
    {}

    /**
     * Returns the value of a preference.
     * @tparam T            Type of the preference
     * @param[in] def       Default value for the preference if it isn't found
     * @param[in] filename  Filename for which the preference is wanted
     * @param[in] function  Function in the file for which the preference is wanted
     * @param[in] nameList  Preference's name components from outermost to innermost
     * @return A pair whose first component is the value for the preference and whose second
     *         component indicates whether or not the preference was found
     */
    template<typename T>
    pair<T, bool> getAs(
            T&                       def,
            const String&            filename,
            const String&            function,
            initializer_list<String> nameList) {
       vector<String> names(2 + nameList.size());
       const auto index = filename.rfind(".");
       names.push_back(filename.substr(0, index));
       names.push_back(function);
       names.insert(names.end(), nameList.begin(), nameList.end());

       try {
           auto pair = getAs<T>(def, names);
           if (!pair.second) {
                ostringstream defStrStrm{};
                defStrStrm << def;
                LOG_INFO("Preference \"%s\" not found. "
                        "Using default value \"%s\"",
                        makeString(nameList).data(), defStrStrm.str().data());
           }
           return pair;
       }
       catch (const exception& ex) {
           throw_with_nested(RUNTIME_ERROR("yaml-cpp failure for preference \""
                   + makeString(names) + "\""));
       }
    }
};

void Preferences::load(const String& pathname)
{
    preferences = shared_ptr<Preferences>(new PrefImpl(pathname));
}

} // namespace
