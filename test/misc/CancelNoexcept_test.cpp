#include <condition_variable>
#include <mutex>
#include <pthread.h>
#include <thread>
#include <unistd.h>

typedef std::mutex              Mutex;
typedef std::lock_guard<Mutex>  Guard;
typedef std::unique_lock<Mutex> Lock;
typedef std::condition_variable Cond;

class Job
{
public:
    void operator()(Mutex& mutex, Cond& cond, bool& ready) noexcept
    {
        {
            Guard guard{mutex};
            ready = true;
            cond.notify_one();
        }
        ::pause();
    }
};

int main(int argc, char** argv)
{
    Job   job;
    Mutex mutex;
    Cond  cond;
    bool  ready;
    std::thread thread(&Job::operator(), &job, std::ref(mutex), std::ref(cond),
            std::ref(ready));

    Lock lock{mutex};
    while (!ready)
        cond.wait(lock);

    ::pthread_cancel(thread.native_handle());
    thread.join();
}
