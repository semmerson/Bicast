#include "IfacePeer.h"

class PeerImpl : public Peer
{};

static void func(SubPeer& peer) {
}

class SubPeerImpl : public SubPeer, public PeerImpl
{
public:
    void start() override {
        func(*this);
    }
};

SubPeer::SubPimpl SubPeer::create() {
    return SubPimpl{new SubPeerImpl()};
}

int main(int argc, char** argv)
{
    auto subPeer = SubPeer::create();
    subPeer->start();
}
