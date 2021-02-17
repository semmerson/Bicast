/**
 * FutureTask.cpp
 *
 *  Created on: May 31, 2017
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

#include <functional>
#include <thread>

struct Exec
{
    struct Task
    {
        std::function<void()> func;
        Task()
            : func{}
        {}
        Task(std::function<void()> func)
            : func{func}
        {}
        void operator()()
        {
            func();
        }
    };
    struct Future
    {
        Task task;
        int  result;
        Future()
            : result{}
        {}
        void setTask(Task& task)
        {
            this->task = task;
        }
    };
    Future submit(std::function<int()> func)
    {
        Future future{};
        Task task([&future,func]{future.result = func();});
        future.setTask(task);
        std::thread thread(task);
        return future;
    }
};
