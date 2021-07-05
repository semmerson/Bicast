#include "config.h"
#include "HycastProto.h"

#include <iostream>
#include <memory>

namespace hycast {

class NoticeQueue
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    NoticeQueue();
};

class PduIdQueue
{
    mutable Mutex mutex;
    mutable Cond  cond;

public:
    PduIdQueue()
        : mutex()
        , cond()
    {}

    PduIdQueue(const PduIdQueue& pduIdQueue) =delete;
    PduIdQueue& operator=(const PduIdQueue& pduIdQueue) =delete;

    ~PduIdQueue() noexcept {
        //Guard guard(mutex);
    }
};

class NoticeQueue::Impl
{
    mutable Mutex mutex;
    PduIdQueue    pduIdQueue;

public:
    Impl()
        : mutex()
        , pduIdQueue()
    {}
};

NoticeQueue::NoticeQueue()
    : pImpl(std::make_shared<Impl>())
{}

} // Namespace

int main(int ac, char** argv)
{
    hycast::NoticeQueue noticeQueue{};
    std::cout << "Success\n";
    return 0;
}
