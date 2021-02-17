/**
 * This file declares the API for exceptions for the Hycast package.
 *
 *   @file: Exception.h
 * @author: Steven R. Emmerson
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

#ifndef EXCEPTION_H_
#define EXCEPTION_H_

namespace hycast {

class Exception {
private:
    const char* file;
    const int   line;
public:
    Exception(
            const char* file,
            const int   line);
    virtual ~Exception() {};        // definition must exist
};

}

#endif /* EXCEPTION_H_ */
