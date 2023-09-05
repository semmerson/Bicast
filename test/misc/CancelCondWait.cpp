/**
 * This file verifies that a thread that's waiting on a condition variable can be cancelled.
 *
 * It can.
 */
#undef NDEBUG
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <thread>
#include <time.h>
#include <unistd.h>

static std::mutex              mutex{};
static std::condition_variable condVar{};
static std::queue<int>         queue{};

static void produce() {
    ::timespec sleepTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 100000;

    for (int i = 0;; ++i) {
        std::lock_guard<std::mutex> guard{mutex};

        queue.push(i);
        condVar.notify_one();
        ::nanosleep(&sleepTime, nullptr);
    }
}

static void consume() {
    auto pred = []{return !queue.empty();};

    for (;;) {
        std::unique_lock<std::mutex> lock{mutex};

        condVar.wait(lock, pred);
        const int i = queue.front();
        queue.pop();
        std::cout << i << '\n';
    }
};

int main(int argc, char** argv) {
    int prevValue;
    assert(::pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &prevValue) == 0);
    assert(::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &prevValue) == 0);

    auto consumerThread = std::thread(&::consume);
    auto producerThread = std::thread(&::produce);

    ::sleep(5);

    ::pthread_cancel(producerThread.native_handle());
    producerThread.join();

    ::pthread_cancel(consumerThread.native_handle());
    consumerThread.join();

    return 0;
}
