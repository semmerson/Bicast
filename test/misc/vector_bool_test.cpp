/**
 * This file 
 *
 *        File: vector_bool_test.cpp
 *  Created on: Nov 2, 2017
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

#include <iostream>
#include <vector>

int main()
{
    std::vector<bool> vb(0);
    long num = 0;
    if (num+1 > vb.size())
        vb.resize(num+1);
    vb[num] = true;
    std::cout << vb.size() << std::endl;
}
