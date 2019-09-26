#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void* runServer(void* arg)
{
    int srvrSock = *(int*)arg;
    int sd = accept(srvrSock, NULL, NULL);
    assert(sd != -1);

    int     status;
    ssize_t nbytes = read(sd, &status, sizeof(status));
    assert(nbytes == sizeof(status));

    nbytes = write(sd, &status, sizeof(status));
    assert(nbytes == sizeof(status));

    close(sd);
    return NULL;
}

int main(int argc, char** argv)
{
    struct sockaddr_in srvrAddr;
    memset(&srvrAddr, 0, sizeof(srvrAddr));
    srvrAddr.sin_family = AF_INET;
    srvrAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    srvrAddr.sin_port = htons(38800);

    //for (int i = 0; ; ++i) {
        //printf("%d\n", i);

        int srvrSock = socket(AF_INET, SOCK_STREAM, 0);
        assert(srvrSock != -1);

        int yes = 1;
        int status = setsockopt(srvrSock, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(yes));
        assert(status == 0);

        status = bind(srvrSock, (struct sockaddr*)&srvrAddr, sizeof(srvrAddr));
        if (status != 0) {
            fprintf(stderr, "bind() failed: %s (%d)\n", strerror(errno), errno);
            exit(1);
        }

        status = listen(srvrSock, 0);
        assert(status == 0);

        pthread_t srvrThread;
        status = pthread_create(&srvrThread, NULL, runServer, &srvrSock);
        assert(status == 0);

        int sd = socket(AF_INET, SOCK_STREAM, 0);
        assert(sd != -1);

        status = connect(sd, (struct sockaddr*)&srvrAddr, sizeof(srvrAddr));
        if (status != 0) {
            fprintf(stderr, "connect() failed: %s (%d)\n", strerror(errno),
                    errno);
            exit(1);
        }

        ssize_t nbytes = write(sd, &status, sizeof(status));
        assert(nbytes == sizeof(status));

        nbytes = read(sd, &status, sizeof(status));
        assert(nbytes == sizeof(status));

        // Makes no difference
        //status = shutdown(sd, SHUT_WR);
        //assert(status == 0);

        close(sd);
        pthread_join(srvrThread, NULL);
        close(srvrSock);
    //}
}
