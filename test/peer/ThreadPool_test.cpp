#undef NDEBUG
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class Job {
public:
    void operator()() { // Trivial
    }
};

class ThreadPool {
    class JobQueue
    {
        std::condition_variable readCond;
        std::condition_variable writeCond;
        std::mutex              jobMutex;
        Job                     job;
        bool                    jobIsSet;
        bool                    done;

    public:
        JobQueue()
            : readCond()
            , writeCond()
            , jobMutex()
            , job()
            , jobIsSet{false}
            , done{false}
        {}

        bool put(Job job) {
            std::unique_lock<std::mutex> lock{jobMutex};

            while (!done && jobIsSet)
                writeCond.wait(lock);

            if (done)
                return false;

            this->job = job;
            jobIsSet = true;
            readCond.notify_one();

            return true;
        }

        bool take(Job job) {
            std::unique_lock<std::mutex> lock{jobMutex};

            while (!done && !jobIsSet)
                readCond.wait(lock);

            if (done)
                return false;

            job = this->job;
            jobIsSet = false;
            writeCond.notify_one();

            return true;
        }

        void setDone() {
            std::lock_guard<std::mutex> guard{jobMutex};
            done = true;
            readCond.notify_all();
            writeCond.notify_all();
        }
    };

    class Worker {
        JobQueue*   jobQueue;
        Job         job;
        std::thread thread;

        Worker(const Worker& worker) =delete;

        Worker(const Worker&& worker) =delete;

        Worker& operator=(const Worker& rhs) =delete;

        Worker& operator=(Worker&& rhs) =delete;

        void operator()() {
            while (jobQueue->take(job))
                job();
        }

    public:
        Worker()
            : jobQueue{nullptr}
            , job{}
            , thread{}
        {}

        ~Worker() noexcept {
            if (thread.joinable())
                thread.join();
        }

        void setJobQueue(JobQueue* jobQueue) {
            this->jobQueue = jobQueue;
            thread = std::thread(&Worker::operator(), this); // Throws if active
        }
    };

    JobQueue             jobQueue;
    std::vector<Worker>  workers;

public:
    ThreadPool()
        : jobQueue()
        , workers(1)
    {
        auto end = workers.end();
        for (auto iter = workers.begin(); iter != end; ++iter)
            iter->setJobQueue(&jobQueue);
    }

    ~ThreadPool() {
        jobQueue.setDone();
    }

    bool run(Job job) {
        return jobQueue.put(job);
    }
};

int main(int argc, char** argv) {
    Job        job{};
    ThreadPool pool{};
    bool       success = pool.run(job);

    assert(success);
}
