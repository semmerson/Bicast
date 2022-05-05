#include <memory>

class Peer
{
public:
    using Pimpl = std::shared_ptr<Peer>;

    static Pimpl createPub();

    static Pimpl createSub();

    virtual ~Peer() {}

    virtual std::string to_string() =0;

    virtual void start() =0;
};
