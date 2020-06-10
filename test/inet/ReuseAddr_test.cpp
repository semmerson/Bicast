#include <arpa/inet.h>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

class Socket {
protected:
    int sd;
public:
    explicit Socket(int sd)
        : sd{sd}
    {
        assert(sd != -1);
    }
    Socket(struct sockaddr_in& sockAddr)
        : Socket(::socket(AF_INET, SOCK_STREAM, 0))
    {}
    virtual ~Socket()
    {
        //::close(sd);
    }
    ssize_t write(const void* buf, const size_t nbytes) const
    {
        return ::write(sd, buf, nbytes);
    }
    ssize_t read(void* buf, const size_t nbytes) const
    {
        return ::read(sd, buf, nbytes);
    }
    void close()
    {
        ::close(sd);
    }
};

class SrvrSock : public Socket {
public:
    SrvrSock(struct sockaddr_in& sockAddr)
        : Socket(sockAddr)
    {
        const int enable = 1;
        int       status = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &enable,
                sizeof(enable));
        assert(status == 0);

        if (bind(sd, reinterpret_cast<struct sockaddr*>(&sockAddr),
                sizeof(sockAddr)) != 0)
            throw std::runtime_error("bind() failure");

        status = listen(sd, 0);
        assert(status == 0);
    }
    Socket accept()
    {
        return Socket{::accept(sd, NULL, NULL)};
    }
};

class ClntSock : public Socket {
public:
    ClntSock(struct sockaddr_in& sockAddr)
        : Socket(sockAddr)
    {
        int status = connect(sd, reinterpret_cast<struct sockaddr*>(&sockAddr),
                sizeof(sockAddr));
        assert(status == 0);
    }
};

static void runServer(SrvrSock& srvrSock) {
    Socket  acceptSock = srvrSock.accept();
    ssize_t status;
    while (acceptSock.read(&status, sizeof(status)) == sizeof(status)) {
        status = acceptSock.write(&status, sizeof(status));
        assert(status == sizeof(status));
    }
}

int main(int argc, char** argv) {
    struct sockaddr_in srvrAddr;
    memset(&srvrAddr, 0, sizeof(srvrAddr));
    srvrAddr.sin_family = AF_INET;
    srvrAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    srvrAddr.sin_port = htons(38800);

    try {
        SrvrSock    srvrSock(srvrAddr);
        std::thread srvrThread(&runServer, std::ref(srvrSock));

        {
            ClntSock clntSock(srvrAddr);

            ssize_t status = clntSock.write(&status, sizeof(status));
            assert(status == sizeof(status));

            status = clntSock.read(&status, sizeof(status));
            assert(status == sizeof(status));

            clntSock.close();
        }

        srvrThread.join();
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
    }
}
