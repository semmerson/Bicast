/**
 * This file tests the pImpl idiom.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: pimpl_test.cpp
 *  Created on: Aug 25, 2017
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include <chrono>
#include <iostream>
#include <memory>

class Counter {
    class Impl {
        int i;
    public:
        Impl() : i{0} {
        }
        void inc() noexcept {
            ++i;
        }
        int get() const noexcept {
            return i;
        }
        void reset() noexcept {
            i = 0;
        }
    };

    std::shared_ptr<Impl> pImpl;

public:
    Counter() : pImpl{new Impl()} {
    }
    void inc() const noexcept {
        pImpl->inc();
    }
    int get() const noexcept {
        return pImpl->get();
    }
    void reset() const noexcept {
        pImpl->reset();
    }
};

static void valFunc(Counter base) {
    base.inc();
}

static void refFunc(Counter& base) {
    base.inc();
}

/*
 * Results:
 *      $ uname -a
 *      Linux localhost.localdomain 3.10.0-514.26.2.el7.x86_64 #1 SMP Tue Jul 4 15:04:05 UTC 2017 x86_64 x86_64 x86_64 GNU/Linux
 *      $ g++ -dumpversion
 *      4.8.5
 *      $ make pimpl_test
 *      /bin/sh ../../libtool  --tag=CXX   --mode=link g++ -std=c++11 -g   -o pimpl_test pimpl_test.o ../../main/libhycast.la -lgtest -lyaml-cpp -lsctp -lpthread 
 *      libtool: link: g++ -std=c++11 -g -o .libs/pimpl_test pimpl_test.o  ../../main/.libs/libhycast.so -lgtest -lyaml-cpp -lsctp -lpthread -Wl,-rpath -Wl,/home/steve/Projects/hycast/lib
 *      $ ./pimpl_test 
 *      valFunc() time/call = 6.22877e-08 s
 *      refFunc() time/call = 1.3568e-08 s
 *
 * Conclusion:
 *      Pass even a Pimpl object as a reference.
 */
int main() {
    typedef std::chrono::steady_clock      Clock;
    typedef std::chrono::time_point<Clock> TimePoint;
    typedef std::chrono::duration<double>  Duration; // Double precision seconds

    Counter   counter{};

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 5000000; ++i)
        valFunc(counter);
    auto seconds = std::chrono::duration_cast<Duration>(Clock::now() - start).count();
    std::cout << "valFunc() time/call = " << seconds/counter.get() << " s" <<
            std::endl;

    counter.reset();

    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 5000000; ++i)
        refFunc(counter);
    seconds = std::chrono::duration_cast<Duration>(Clock::now() - start).count();
    std::cout << "refFunc() time/call = " << seconds/counter.get() << " s" <<
            std::endl;
}
