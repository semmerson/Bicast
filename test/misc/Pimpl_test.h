/**
 * This file tests the pImpl idiom.
 *
 *  @file:  Pimpl_test.h
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

#ifndef TEST_MISC_PIMPL_TEST_H_
#define TEST_MISC_PIMPL_TEST_H_

#include <memory>

namespace {

class Ref
{
    class Impl;
    std::shared_ptr<Impl> pImpl;

public:
    Ref();

    void func();
};

} // namespace

#endif /* TEST_MISC_PIMPL_TEST_H_ */
