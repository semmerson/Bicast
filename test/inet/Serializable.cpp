/**
 * This file determines if an interface can require a *static* virtual method.
 *
 * Doesn't work:
 *    steve@gilda inet$ gcc -std=c++11 -c Serializable.cpp
 *    Serializable.cpp:41:53: error: member ‘deSerialize’ cannot be declared both virtual and static
 *         static virtual void* deSerialize(const int fd) =0;
 *                                                         ^
 *    Serializable.cpp:46:18: error: ‘static void* Obj::deSerialize(int)’ cannot be declared
 *         static void* deSerialize(const int fd) override {
 *                      ^
 *    Serializable.cpp:41:26: error:   since ‘virtual void* Serializable::deSerialize(int)’ declared in base class
 *         static virtual void* deSerialize(const int fd) =0;
 *                              ^
 *    Serializable.cpp:46:18: error: ‘static void* Obj::deSerialize(int)’ marked override, but does not override
 *         static void* deSerialize(const int fd) override {
 *                      ^
 *
 *  @file:  Serializable.cpp
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

class Serializable {
public:
    virtual ~Serializable () {}

    static virtual void* deSerialize(const int fd) =0;
};

class Obj : public Serializable {
public:
    static void* deSerialize(const int fd) override { // Removal of `static` doesn't work
        return nullptr;
    }
};
