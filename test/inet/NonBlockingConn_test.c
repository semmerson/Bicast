#define _XOPEN_SOURCE 700

#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

static pthread_barrier_t barrier;

static void* runServer(void* arg)
{
    int serverSd = *(int*)arg;

    int clientSd = accept(serverSd, NULL, NULL);
    assert(clientSd >= 0);

    int status = pthread_barrier_wait(&barrier);
    assert(status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);

    assert(close(clientSd) == 0);
    return NULL;
}

static void testAsyncConn(struct sockaddr* serverAddr, socklen_t addrSize)
{
    // Create socket for server
    int status;
    int serverSd =  socket(serverAddr->sa_family, SOCK_STREAM, 0);
    assert(serverSd >= 0);

    // Initialize socket for serving
    const int yes = 1;
    assert(setsockopt(serverSd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == 0);
    assert(bind(serverSd, serverAddr, addrSize) == 0);
    assert(listen(serverSd, 0) == 0);

    // Create server thread
    pthread_t serverThread;
    assert(pthread_create(&serverThread, NULL, runServer, &serverSd) == 0);
    assert(pthread_detach(serverThread) == 0);

    // Create socket for client
    int clientSd = socket(serverAddr->sa_family, SOCK_STREAM, 0);
    assert(clientSd >= 0);

    // Set client socket to non-blocking
    int sockFlags = fcntl(clientSd, F_GETFL, 0);
    assert(sockFlags >= 0);
    assert(fcntl(clientSd, F_SETFL, sockFlags | O_NONBLOCK) >= 0);

    // Initiate connection to server
    status = connect(clientSd, serverAddr, addrSize);
    assert(status == -1 && errno == EINPROGRESS);

    // Wait until connected
    struct pollfd pfd = {.fd=clientSd, .events=POLLOUT};
    status = poll(&pfd, 1, -1); // -1 => indefinite timeout
    assert(status == 1);

    /*
     * Check client socket status. NB: POLLHUP and POLLERR are also set.
     */
    assert(pfd.revents & POLLOUT);

    // Signal server to terminate
    status = pthread_barrier_wait(&barrier);
    assert(status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);

    // Close open sockets
    assert(close(clientSd) == 0);
    assert(close(serverSd) == 0);
}

int main(int argc, char** argv)
{
    const in_port_t port = htons(38801);
    assert(pthread_barrier_init(&barrier, NULL, 2) == 0);

    struct sockaddr_in ipv4Addr = {.sin_family=AF_INET, .sin_addr=htonl(INADDR_LOOPBACK),
           .sin_port=port};
    testAsyncConn((struct sockaddr*)&ipv4Addr, sizeof(ipv4Addr));

    struct sockaddr_in6 ipv6Addr = {.sin6_family=AF_INET6, .sin6_addr=IN6ADDR_LOOPBACK_INIT,
           .sin6_port=port};
    testAsyncConn((struct sockaddr*)&ipv6Addr, sizeof(ipv6Addr));

    assert(pthread_barrier_destroy(&barrier) == 0);
}
