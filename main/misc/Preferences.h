/**
 * This file declares a
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

#ifndef MAIN_MISC_CONTEXT_H_
#define MAIN_MISC_CONTEXT_H_

#include <memory>

namespace hycast {

using String = std::string;

/// Interface for a preferences object
class Preferences
{
public:
    /// Smart pointer to the implementation
    using Pimpl = std::shared_ptr<Preferences>;

    /**
     * Loads global preferences from a file.
     *
     * @param[in] pathname  Pathname of file
     */
    static void load(const String& pathname);

    virtual ~Preferences() {};

    /**
     * Returns the value associated with a preference if it exists or a
     * default value if it doesn't.
     *
     * @tparam    T         Type of value
     * @param[in] def       Default value if preference doesn't exist
     * @param[in] filename  Name of the file for which the preference is wanted
     * @param[in] function  Name of the function in the file for which the preference is wanted
     * @param[in] names     String components of preference's name from
     *                      outermost to innermost
     * @return              Pair whose first element is the value and whose
     *                      second element is false   if the default value
     *                      was returned and `true` otherwise
     * @throw RuntimeError  Most likely the preference's value isn't convertible
     *                      to a `T`
     */
    template<typename T>
    std::pair<T, bool> getAs(
            const T&                            def,
            const String&                       filename,
            const String&                       function,
            const std::initializer_list<String> names);
};

/**
 * Convenience macro for obtaining a preference's value. The first two
 * components of the preference's name are the name of the file (minus the file
 * extension) and the name of the function.
 *
 * @see Preferences::getAs()
 */
#define PREF_GET_AS(type, def, names) \
        preferences->getAs<type>(def, _FILE__, __func__, names)

/// Global preferences
Preferences::Pimpl preferences;

} // namespace

#endif /* MAIN_MISC_CONTEXT_H_ */
