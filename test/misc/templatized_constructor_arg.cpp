/**
 * This file tests a constructor of a template class whose argument is similarly
 * templatized.
 *
 *        File: templatized_constructor_arg.cpp
 *  Created on: Jun 7, 2017
 *      Author: Steven R. Emmerson
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

template<class T> struct Foo {
    Foo(T value)
    {}
    T get() {
        return T{};
    }
};

template<> struct Foo<void> {
    void get() {}
};

template<class T> struct Bar {
    Foo<T> foo;
    Bar(Foo<T>& foo)
        : foo{foo}
    {}
};

template<> struct Bar<void> {
    Foo<void> foo;
#if 0
    Bar(Foo<void> fooArg)
        : foo{fooArg} // Error: too many initializers for `foo`
    {}
#else
    Bar(Foo<void> fooArg)
        : foo(fooArg)
    {}
#endif
#if 0
    Bar(Foo<void>& fooArg)
        : foo{fooArg} // Error: too many initializers for `foo`
    {}
#else
    Bar(Foo<void>& fooArg)
        : foo(fooArg)
    {}
#endif
};
