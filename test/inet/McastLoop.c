#undef NDEBUG
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#if 0
    #define IFACE_SPEC "127.0.0.1"      // Loopback interface
    #define IFACE_INDEX 1               // Index of above interface
#else
    #define IFACE_SPEC "192.168.58.132" // Public interface
    #define IFACE_INDEX 2               // Index of above interface
#endif

static int                sendSd; // Socket descriptor for sending
static int                recvSd; // Socket descriptor for receiving
static int                sendVal = 1;
static int                recvVal = 0;
static struct sockaddr_in mcastSockAddr; // Multicast group socket address

static void Send()
{
    assert(sendto(sendSd, &sendVal, sizeof(sendVal), 0, (struct sockaddr*)&mcastSockAddr,
            sizeof(mcastSockAddr)) == sizeof(sendVal));
    //assert(write(sendSd, &sendVal, sizeof(sendVal)) == sizeof(sendVal));
}

static void* Recv(void* arg)
{
    assert(read(recvSd, &recvVal, sizeof(recvVal)) == sizeof(recvVal)); // Hangs here
    assert(recvVal == sendVal);
    return NULL;
}

#if 0
// Works for loopback interface; hangs for public interface
static void singleThread()
{
    Send();
    Recv(NULL);
}

// Works for both interfaces
static void multipleProcesses()
{
    const pid_t pid = fork();
    assert(pid != -1);
    if (pid) {
        Send();     // Parent
    }
    else {
        Recv(NULL); // Child
    }
}
#endif

// Works for loopback interface; hangs for public interface
static void multipleThreads()
{
    pthread_t recvThread;
    assert(pthread_create(&recvThread, NULL, Recv, NULL) == 0);

    //sleep(1); // Doesn't fix hanging

    Send();

    assert(pthread_join(recvThread, NULL) == 0);
}

int main(int argc, char** argv)
{
    static const unsigned char charNo = 0;
    static const unsigned char charYes = 1;
    static const int           intYes = 1;
    struct in_addr             ifaceAddr = {.s_addr=inet_addr(IFACE_SPEC)};

    mcastSockAddr.sin_family=AF_INET,
    mcastSockAddr.sin_addr.s_addr=inet_addr("232.1.1.1"), // Source-specific multicast address
    mcastSockAddr.sin_port = htons(38800);

    // Multicast sending socket
    {
        sendSd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        assert(sendSd != -1);

        // Disable local loopback of multicast datagrams. Not strictly necessary.
        assert(setsockopt(sendSd, IPPROTO_IP, IP_MULTICAST_LOOP, &charYes, sizeof(charNo)) == 0);

        // Set the interface to be used for outgoing multicast
        assert(setsockopt(sendSd, IPPROTO_IP, IP_MULTICAST_IF, &ifaceAddr, sizeof(ifaceAddr)) == 0);

        // Connect to the multicast group
        //assert(connect(sendSd, (struct sockaddr*)&mcastSockAddr, sizeof(mcastSockAddr)) == 0);
    }

    // Multicast receiving socket
    {
        recvSd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        assert(recvSd != -1);

        // Ensure reception by all subscribing sockets. Not strictly necessary.
        assert(setsockopt(recvSd, SOL_SOCKET, SO_REUSEADDR, &intYes, sizeof(intYes)) == 0);
        // Bind the socket to the multicast group address
        assert(bind(recvSd, (struct sockaddr*)&mcastSockAddr, sizeof(mcastSockAddr)) == 0);

        // Join the multicast group
        struct group_source_req mreq = {.gsr_interface=IFACE_INDEX};
        memcpy(&mreq.gsr_group, (struct sockaddr*)&mcastSockAddr, sizeof(mcastSockAddr));
        struct sockaddr_in ifaceSockAddr = {
                .sin_family=AF_INET,
                .sin_addr=ifaceAddr,
                .sin_port=0}; // Port number is ignored
        memcpy(&mreq.gsr_source, &ifaceSockAddr, sizeof(ifaceSockAddr));
        assert(setsockopt(recvSd, IPPROTO_IP, MCAST_JOIN_SOURCE_GROUP, &mreq, sizeof(mreq)) == 0);
    }

    //singleThread();      // Hangs for public interface
    multipleThreads();   // Hangs for public interface
    //multipleProcesses(); // Works for both interfaces
}
