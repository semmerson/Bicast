#include <iostream>
#include <memory>

struct Socket {
    struct Impl;

    std::shared_ptr<Impl> pImpl;

    Socket() = default;

    Socket(const int sd);
};

struct Socket::Impl {
    int sd;

    Impl(const int sd) : sd{sd} {}

    ~Impl() {
        std::cout << "Closing socket\n";
    }
};

Socket::Socket(const int sd) : pImpl(new Impl(sd)) {}

struct Xprt {
    struct Impl;

    std::shared_ptr<Impl> pImpl;

    Xprt() = default;

    Xprt(Socket sock);
};

struct Xprt::Impl {
    Socket sock;

    Impl() = default;

    Impl(Socket sock) : sock(sock) {}
};

Xprt::Xprt(Socket sock) : pImpl(new Impl(sock)) {}

Xprt memo{};

void saveXprt(Xprt xprt) {
    std::cout << "      saveXprt(): xprt.pImpl->sock.pImpl.use_count() = " << xprt.pImpl->sock.pImpl.use_count() << '\n';
    memo = xprt;
    std::cout << "      saveXprt(): xprt.pImpl->sock.pImpl.use_count() = " << xprt.pImpl->sock.pImpl.use_count() << '\n';
    std::cout << "      saveXprt(): memo.pImpl->sock.pImpl.use_count() = " << memo.pImpl->sock.pImpl.use_count() << '\n';
}

void processSock(Socket sock) {
    std::cout << "   processSock(): sock.pImpl.use_count() = " << sock.pImpl.use_count() << '\n';
    Xprt   xprt{sock};
    std::cout << "   processSock(): xprt.pImpl->sock.pImpl.use_count() = " << xprt.pImpl->sock.pImpl.use_count() << '\n';
    saveXprt(xprt);
    std::cout << "   processSock(): xprt.pImpl->sock.pImpl.use_count() = " << xprt.pImpl->sock.pImpl.use_count() << '\n';
    std::cout << "   processSock(): memo.pImpl->sock.pImpl.use_count() = " << memo.pImpl->sock.pImpl.use_count() << '\n';
}

int main(int argc, char** argv) {
    {
        Socket sock{1};
        std::cout << "main(): sock.pImpl.use_count() = " << sock.pImpl.use_count() << '\n';
        processSock(sock);
        std::cout << "main(): sock.pImpl.use_count() = " << sock.pImpl.use_count() << '\n';
    }
    std::cout << "main(): memo.pImpl->sock.pImpl.use_count() = " << memo.pImpl->sock.pImpl.use_count() << '\n';
    std::cout << "Terminating\n";
}
