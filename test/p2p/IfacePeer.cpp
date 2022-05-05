#include "IfacePeer.h"
#include <iostream>
#include <array>

static void func(Peer& peer) {
    std::cout << peer.to_string() << '\n';
}

class PeerImpl : public Peer
{
public:
    std::string to_string() override =0;

    void start() override =0;
};

class PubPeerImpl : public PeerImpl
{
public:
    std::string to_string() {
        return "PubPeerImpl";
    }

    void start() override {
        func(*this);
    }
};

class SubPeerImpl : public PeerImpl
{
public:
    std::string to_string() {
        return "SubPeerImpl";
    }

    void start() override {
        func(*this);
    }
};

Peer::Pimpl Peer::createPub() {
    return Pimpl{new PubPeerImpl()};
}

Peer::Pimpl Peer::createSub() {
    return Pimpl{new SubPeerImpl()};
}

int main(int argc, char** argv)
{
    std::array<Peer::Pimpl, 2> array;
    array[0] = Peer::createPub();
    array[1] = Peer::createSub();
    array[0]->start(); // Eclipse warning is bogus
    array[1]->start(); // Eclipse warning is bogus
}
