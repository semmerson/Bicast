/**
 * Joins a source-specific multicast group and prints packets to stdout.
 *
 * @author Steven R. Emmerson
 */

#define _XOPEN_SOURCE 600
#define _BSD_SOURCE // For `struct ip_mreq_source`
#define __USE_MISC // For `struct ip_mreq_source`

#include "SsmTest.h"

#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static const char*        sourceId;       ///< IP address of SSM source
static struct in_addr     srcIpAddr;      ///< IP address of SSM source
static const char*        ifaceIpAddr;    ///< IP address of interface to use
static struct in_addr     ifIpAddr;       ///< IP address of interface to use
static struct sockaddr_in ssmSockAddr;    ///< SSM group socket address
static bool               verbose;        ///< Verbose mode?

static bool
getIpv4Addr(const char* const     name,
            struct in_addr* const addr)
{
    bool             success = false;
    struct addrinfo  hints = {};
    struct addrinfo* list;

    hints.ai_family = AF_INET;
    hints.ai_socktype = 0;

    if (getaddrinfo(name, NULL, &hints, &list)) {
        (void)fprintf(stderr, "Couldn't get information on host \"%s\": %s\n",
                name, strerror(errno));
    }
    else {
        for (struct addrinfo* entry = list; entry != NULL;
                entry = entry->ai_next) {
            if (entry->ai_family == SSM_FAMILY) {
                *addr = ((struct sockaddr_in*)entry->ai_addr)->sin_addr;
                success = true;
                break;
            }
        }

        freeaddrinfo(list);
    }

    return success;
}

static void
usage(const char* const progname)
{
    (void)fprintf(stderr,
"Usage:\n"
"    %s [-v] <srcAddr> <iface>\n"
"where:\n"
"    <srcAddr> Hostname or IPv4 address of source host\n"
"    <iface>   IPv4 address of interface to receive multicast on\n"
"    -v        Verbose output\n",
    progname);
}

/**
 * Sets the runtime parameters.
 *
 * @param[in]  argc        Number of command-line arguments
 * @param[in]  argv        Command-line arguments
 * @retval `true`  Success
 * @retval `false` Failure
 */
static bool
setRunPar(
        int                   argc,
        char* const* restrict argv)
{
    bool success = true;
    int  ch;

    while (success && (ch = getopt(argc, argv, "v")) != -1) {
        switch (ch) {
        case 'v': {
            verbose = true;
            break;
        }
        default:
            usage(basename(argv[0]));
            success = false;
            break;
        }
    }

    if (success) {
        success = false;

        if (optind >= argc) {
            (void)fprintf(stderr, "Too few operands\n");
            usage(basename(argv[0]));
        }
        else {
            sourceId = argv[optind++];
            if (inet_pton(SSM_FAMILY, sourceId, &srcIpAddr) != 1 &&
                    !getIpv4Addr(sourceId, &srcIpAddr)) {
                (void)fprintf(stderr, "Couldn't convert source ID \"%s\" into an "
                        "IPv4 address\n", sourceId);
                usage(basename(argv[0]));
            }
            else if (optind >= argc) {
                (void)fprintf(stderr, "Too few operands\n");
                usage(basename(argv[0]));
            }
            else {
                ifaceIpAddr = argv[optind++];
                if (inet_pton(SSM_FAMILY, ifaceIpAddr, &ifIpAddr) != 1) {
                    (void)fprintf(stderr, "Couldn't parse interface IP address "
                            "\"%s\": %s\n", ifaceIpAddr, strerror(errno));
                    usage(basename(argv[0]));
                }
                else {
                    // Set SSM group socket address
                    ssmSockAddr.sin_family = SSM_FAMILY;
                    ssmSockAddr.sin_port = htons(SSM_PORT);

                    if (inet_pton(ssmSockAddr.sin_family, SSM_IP_ADDR,
                            &ssmSockAddr.sin_addr.s_addr) != 1) {
                        (void)fprintf(stderr, "Couldn't parse SSM IP address "
                                "\"%s\": %s\n", SSM_IP_ADDR, strerror(errno));
                    }
                    else if (optind < argc) {
                        (void)fprintf(stderr, "Too many operands\n");
                        usage(basename(argv[0]));
                    }
                    else {
                        success = true;
                    }
                }
            }
        }
    }

    return success;
}

/**
 * Joins a socket to a source-specific multicast group on a network interface.
 *
 * @param[in] sock        UDP Socket descriptor
 * @retval `true`   Success
 * @retval `false`  Failure
 */
static bool
joinSsm(const int sock)
{
    bool                  success = false;
    struct ip_mreq_source mreq = { // SSM-specific structure
            .imr_sourceaddr = srcIpAddr,
            .imr_multiaddr  = ssmSockAddr.sin_addr,
            .imr_interface  = ifIpAddr};

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, &mreq,
            sizeof(mreq)) != 0) {
        (void)fprintf(stderr, "Couldn't join socket %d to SSM group \"%s\" "
                "on interface \"%s\" with source \"%s\": %s\n", sock,
                SSM_SOCK_ADDR, ifaceIpAddr, sourceId, strerror(errno));
    }
    else {
        if (verbose)
            (void)printf("Joined socket %d to SSM group \"%s\" "
                    "on interface \"%s\" with source \"%s\"\n", sock,
                    SSM_SOCK_ADDR, ifaceIpAddr, sourceId);
        success = true;
    }

    return success;
}

/**
 * Configures a socket for receiving source-specific multicast.
 *
 * @param[in] sock        Socket descriptor
 * @retval    `true`      If and only if success
 */
static bool
configSock(const int sock)
{
    bool success = false;

    // Allow multiple sockets to use the same port number
    const int yes = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
        perror("Couldn't allow multiple sockets to use the same port number");
    }
    else {
        /*
         * Bind the local endpoint of the socket to the address and port
         * number of the multicast group.
         */
        if (bind(sock, (struct sockaddr*)&ssmSockAddr, sizeof(ssmSockAddr)) != 0) {
            (void)fprintf(stderr, "Couldn't bind socket %d to SSM group address \"%s:%d\": %s\n",
                    sock, SSM_IP_ADDR, SSM_PORT, strerror(errno));
        }
        else {
            if (verbose)
                (void)printf("Bound socket %d to \"%s\"\n", sock,
                        SSM_SOCK_ADDR);

            // Join socket to multicast group on interface
            success = joinSsm(sock);
        }
    }

    return success;
}

static bool
printPackets(const int  sock)
{
    int success = false;

    if (verbose)
        (void)printf("Receiving from socket %d bound to \"%s\"\n", sock,
                SSM_SOCK_ADDR);

    // Enter a receive-then-print loop
    for (;;) {
        char          msgbuf[65507]; // Maximum UDP payload
        const ssize_t nbytes = recv(sock, msgbuf, sizeof(msgbuf), 0);

        if (nbytes < 0) {
            perror("Couldn't receive packet");
            break;
        }
        if (nbytes == 0) {
            success = true;
            break;
        }

        (void)printf("%.*s\n", nbytes, msgbuf);
    }

    return success;
}

int
main(int argc, char *argv[])
{
    bool success = false;

    if (setRunPar(argc, argv)) {
        int sd = socket(SSM_FAMILY, SOCK_DGRAM, 0);

        if (sd < 0) {
            perror("Couldn't create UDP socket");
        }
        else {
            success = configSock(sd) && printPackets(sd);
            (void)close(sd);
        }
    }

    return success ? 0 : 1;
}
