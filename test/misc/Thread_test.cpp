/**
 * This file tests class `Thread`.
 *
 *       File: Thread_test.cpp
 * Created On: May 18, 2017
 *     Author: Steven R. Emmerson
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
#include "config.h"

#include "error.h"
#include "Thread.h"

#include <atomic>
#include <climits>
#include <condition_variable>
#include <exception>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <random>
#include <unistd.h>

namespace {

static int arg = 0;

/// The fixture for testing class `Thread`
class ThreadTest : public ::testing::Test
{
protected:
    typedef hycast::Thread::Id       ThreadId;
    typedef std::mutex               Mutex;
    typedef std::lock_guard<Mutex>   LockGuard;
    typedef std::unique_lock<Mutex>  UniqueLock;
    typedef std::condition_variable  Cond;
    class Receiving {
        hycast::Thread     p2pMgrThread;
        hycast::Thread     peerAddrThread;
        std::exception_ptr exception;
        Mutex              mutex;
        Cond               cond;
    public:
        Receiving() :
            p2pMgrThread{},
            peerAddrThread{},
            exception{}
        {
            hycast::Cue cue{};
            p2pMgrThread = hycast::Thread{&Receiving::runP2pMgr, this, cue};
            hycast::Canceler canceler{};
            cue.wait();
        }
        ~Receiving() {
            p2pMgrThread.cancel();
            p2pMgrThread.join();
        }
        static void stopPeerAddr(void* arg) {
            auto pImpl = static_cast<Receiving*>(arg);
            pImpl->peerAddrThread.cancel();
            pImpl->peerAddrThread.join();
        }
        void runPeerAddr() {
            try {
                try {
                    try {
                        /*
                         * If the current thread is canceled while its handling
                         * the exception, then terminate() will be called.
                         */
                        throw hycast::SYSTEM_ERROR("Faux system error", 1);
                    }
                    catch (const std::exception& ex) {
                        std::throw_with_nested(hycast::RUNTIME_ERROR(
                                "Faux outer exception"));
                    }
                }
                catch (const std::exception& ex) {
                    hycast::log_error(ex);
                }
            }
            catch (std::exception& ex) {
                hycast::log_error(ex);
                LOG_ERROR("runPeerAddr() failed");
                LockGuard lock{mutex};
                exception = std::current_exception();
                cond.notify_all();
            }
        }
        void runP2pMgr(hycast::Cue& cue) {
            cue.cue();
            THREAD_CLEANUP_PUSH(stopPeerAddr, this);
            peerAddrThread = hycast::Thread{[this]{runPeerAddr();}};
            UniqueLock lock{mutex};
            while (!exception) {
                hycast::Canceler canceler{};
                cond.wait(lock);
            }
            THREAD_CLEANUP_POP(true);
        }
    };

    std::mutex                mutex;
    std::condition_variable   cond;
    hycast::Thread::Id        threadId;
    std::atomic_bool          cleanupCalled;
    bool                      callableCalled;
    hycast::Thread            p2pMgrThread;

    ThreadTest()
        : mutex{}
        , cond{}
        , threadId{}
        , cleanupCalled{false}
        , callableCalled{false}
    {}

    void memberFunc(const int i)
    {
        arg = i;
    }

    void markAndNotify()
    {
        LockGuard lock{mutex};
        threadId = hycast::Thread::getId();
        callableCalled = true;
        cond.notify_one();
    }

    void printArgs(int one, int two, int three)
    {
        std::cout << "one=" << one << '\n';
        std::cout << "two=" << two << '\n';
        std::cout << "three=" << three << '\n';
        markAndNotify();
    }

    void markNotifyAndPause()
    {
        markAndNotify();
        hycast::Canceler canceler{};
        ::pause();
    }

    void waitOnCallable()
    {
        UniqueLock lock(mutex);
        while (!callableCalled) {
            hycast::Canceler canceler{};
            cond.wait(lock);
        }
    }

    void testCancelState()
    {
        bool enabled = hycast::Thread::enableCancel(true);
        EXPECT_FALSE(enabled);
        enabled = hycast::Thread::enableCancel(false);
        EXPECT_TRUE(enabled);
        markAndNotify();
        hycast::Canceler canceler{};
        ::pause();
    }

    static void cleanup(void* arg)
    {
        ThreadTest* instance  = static_cast<ThreadTest*>(arg);
        EXPECT_NE(ThreadId{}, instance->threadId);
        EXPECT_FALSE(instance->cleanupCalled);
        instance ->cleanupCalled = true;
    }
};

// Tests cancelability state
TEST_F(ThreadTest, CancelState)
{
    bool enabled = hycast::Thread::enableCancel(false);
    EXPECT_TRUE(enabled);
    enabled = hycast::Thread::enableCancel(true);
    EXPECT_FALSE(enabled);
}

// Tests default construction
TEST_F(ThreadTest, DefaultConstruction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{};
    std::cout << "Default p2pMgrThread-ID: " << hycast::Thread::Id{} << '\n';
    EXPECT_EQ(0, hycast::Thread::size());
    EXPECT_NO_THROW(thread.id());
    EXPECT_EQ(ThreadId{}, thread.id());
    EXPECT_NO_THROW(thread.cancel());
}

static void staticFunc(const int i)
{
    arg = i;
}

// Tests static function construction
TEST_F(ThreadTest, StaticFuncConstruction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{&staticFunc, 1};
    EXPECT_EQ(1, hycast::Thread::size());
    thread.join();
    EXPECT_EQ(1, arg);
}

// Tests member function construction
TEST_F(ThreadTest, MemberFuncConstruction)
{
    ASSERT_EQ(0, hycast::Thread::size());

    hycast::Thread thread{&::ThreadTest_MemberFuncConstruction_Test::memberFunc,
            this, 1};
    EXPECT_EQ(1, hycast::Thread::size());
    thread.join();
    EXPECT_EQ(1, arg);

#if 0
    // THE FOLLOWING LINE WON'T COMPILE. THE ABOVE FORM MUST BE USED.
    p2pMgrThread = hycast::Thread{&this->memberFunc, this, 2};
    EXPECT_EQ(1, hycast::Thread::size());
    p2pMgrThread.join();
    EXPECT_EQ(2, arg);
#endif
}

// Tests construction
TEST_F(ThreadTest, Construction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{}};
    EXPECT_EQ(1, hycast::Thread::size());
}

#if 0
// Tests copy construction
// THERE IS NO COPY CONSTRUCTOR
TEST_F(ThreadTest, CopyConstruction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread1{[]{}};
    EXPECT_EQ(1, hycast::Thread::size());
    hycast::Thread thread2{thread1};
    EXPECT_EQ(1, hycast::Thread::size());
    EXPECT_EQ(thread1.id(), thread2.id());
}
#endif

// Tests move construction
TEST_F(ThreadTest, MoveConstruction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{hycast::Thread{[]{}}};
    EXPECT_EQ(1, hycast::Thread::size());
}

// Tests copy assignment
TEST_F(ThreadTest, CopyAssignment)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread1{};
    hycast::Thread thread2{[]{}};
    EXPECT_NE(thread1.id(), thread2.id());
    EXPECT_EQ(1, hycast::Thread::size());
    thread1 = thread2; // Copy assignment
    EXPECT_EQ(thread1.id(), thread2.id());
    EXPECT_EQ(1, hycast::Thread::size());
}

// Tests move assignment
TEST_F(ThreadTest, MoveAssignment)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{};
    ThreadId threadId = thread.id();
    thread = hycast::Thread{[]{}}; // Move assignment
    EXPECT_NE(threadId, thread.id());
    EXPECT_EQ(1, hycast::Thread::size());
}

// Tests thread number
TEST_F(ThreadTest, ThreadNumber)
{
    ASSERT_EQ(0, hycast::Thread::size());
    auto thread1 = hycast::Thread{[]{hycast::Canceler canceler{}; ::pause();}};
    auto thread2 = hycast::Thread{[]{hycast::Canceler canceler{}; ::pause();}};
    EXPECT_LT(thread1.threadNumber(), thread2.threadNumber());
    thread1.cancel();
    thread1.join();
    auto thread3 = hycast::Thread{[]{hycast::Canceler canceler{}; ::pause();}};
    EXPECT_LT(thread3.threadNumber(), thread2.threadNumber());
}

// Tests id() of joined p2pMgrThread
TEST_F(ThreadTest, IdOfJoinedThread)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{}};
    auto ident = thread.id();
    thread.join();
    EXPECT_NE(ident, thread.id());
    EXPECT_EQ(ThreadId{}, thread.id());
}

// Tests usage
TEST_F(ThreadTest, Usage)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{printArgs(1, 2, 3);}};
    EXPECT_EQ(1, hycast::Thread::size());
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests instance cancellation
TEST_F(ThreadTest, InstanceCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{markNotifyAndPause();}};
    auto id = thread.id();
    waitOnCallable();
    thread.cancel();
    thread.join();
    EXPECT_EQ(id, threadId);
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests static cancellation
TEST_F(ThreadTest, StaticCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{markNotifyAndPause();}};
    auto id = thread.id();
    waitOnCallable();
    hycast::Thread::cancel(id);
    thread.join();
    EXPECT_EQ(id, threadId);
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests destructor cancellation
TEST_F(ThreadTest, DestructorCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{hycast::Canceler canceler{}; ::pause();}};
    EXPECT_EQ(1, hycast::Thread::size());
}

// Tests canceling completed p2pMgrThread
TEST_F(ThreadTest, CancelDoneThread)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{markAndNotify();}};
    waitOnCallable();
    thread.cancel();
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests move assignment and non-member cancellation
TEST_F(ThreadTest, MoveAssignmentAndNonMemberCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{};
    thread = hycast::Thread{[this]{markNotifyAndPause();}};
    auto id = thread.id();
    waitOnCallable();
    EXPECT_NO_THROW(hycast::Thread::cancel(thread.id()));
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
    EXPECT_TRUE(callableCalled);
    EXPECT_EQ(id, threadId);
}

// Tests setting cancelability
TEST_F(ThreadTest, SettingCancelability)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{testCancelState();}};
    waitOnCallable();
    thread.cancel();
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests p2pMgrThread cleanup routine
TEST_F(ThreadTest, ThreadCleanupRoutine)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{};
    thread = hycast::Thread{[this] {
            THREAD_CLEANUP_PUSH(cleanup, this);
            markAndNotify();
            hycast::Canceler canceler{};
            ::pause();
            THREAD_CLEANUP_POP(false);
    }};
    auto id = thread.id();
    waitOnCallable();
    EXPECT_NO_THROW(hycast::Thread::cancel(thread.id()));
    thread.join();
    EXPECT_NE(ThreadId{}, threadId);
    EXPECT_TRUE(cleanupCalled);
    EXPECT_EQ(0, hycast::Thread::size());
    EXPECT_EQ(id, threadId);
}

// Tests cancellation loop
TEST_F(ThreadTest, CancellationLoop)
{
    //std::set_terminate([]{::pause();});
    int i;
    try {
        for (i = 0; i < 500; ++i) {
            ASSERT_EQ(0, hycast::Thread::size());
            hycast::Thread thread{[]{}};
            thread.cancel();
            thread.join();
        }
        for (; i < 1000; ++i) {
            ASSERT_EQ(0, hycast::Thread::size());
            hycast::Thread thread{[this]{markNotifyAndPause();}};
            thread.cancel();
            thread.join();
        }
        for (; i < 1500; ++i) {
            ASSERT_EQ(0, hycast::Thread::size());
            hycast::Thread thread{[this]{markNotifyAndPause();}};
            waitOnCallable();
            thread.cancel();
            thread.join();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Loop failed on iteration " << i << '\n';
    }
}

// Tests execution of a bunch of threads
TEST_F(ThreadTest, BunchOfThreads) {
    //std::set_terminate([]{std::cout << "terminate() called\n"; ::pause();});
    std::default_random_engine generator{};
    std::uniform_int_distribution<useconds_t> distribution{0, 100000};
    hycast::Thread threads[200];
    for (int i = 0; i < 200; ++i)
        threads[i] = hycast::Thread([&generator,&distribution]() mutable
                {::usleep(distribution(generator));});
    usleep(50000);
}

// Simulates problem in ShipRecv_test.cpp
TEST_F(ThreadTest, Receiving) {
    Receiving{};
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
