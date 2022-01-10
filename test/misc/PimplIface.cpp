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

#include "PimplIface.h"

class Abstract::Impl {
public:
    virtual ~Impl() {};

    virtual void foo() =0;
};

Abstract::Abstract(Impl* const impl)
    : pImpl(impl)
{}

/******************************************************************************/

class Derived final : public Abstract::Impl
{
private:
    int arg;

public:
    Derived(int arg)
        : Impl{}
        , arg(arg)
    {}

    ~Derived()
    {}

    void foo() override {
    }
};

Abstract Abstract::create(int arg) {
    return Abstract(new Derived(arg));
}

void Abstract::foo() {
    pImpl->foo();
}

/******************************************************************************/

int main(int argc, char** argv)
{
    auto abstract = Abstract::create(1);
    abstract.foo();
    return 0;
}
