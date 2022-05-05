#include <memory>

class Peer
{
public:
    using Pimpl = std::shared_ptr<Peer>;

    static Pimpl create();

    virtual ~Peer() {}

    virtual void start() =0;
};

/******************************************************************************/

class SubPeer : public Peer
{
public:
    using SubPimpl = std::shared_ptr<SubPeer>;

    static SubPimpl create();
};
