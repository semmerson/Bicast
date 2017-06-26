/**
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 * FutureTask.cpp
 *
 *  Created on: May 31, 2017
 *      Author: Steven R. Emmerson
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
