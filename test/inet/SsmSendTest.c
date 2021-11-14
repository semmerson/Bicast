/**
 * SsmSendTest.c -- multicasts a string to a source-specific multicast group
 * once a second
 *
 * @author: Steven R Emmerson
 */

#define _XOPEN_SOURCE 700

#include "SsmTest.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DEF_IFACE_IP_ADDR "0.0.0.0" ///< Default interface address
#define DEF_TTL           64        ///< Default time-to-live

static const char*        ifaceIpAddr = DEF_IFACE_IP_ADDR;
static struct in_addr     ifIpAddr;        ///< IP address of interface
static struct sockaddr_in ssmSockAddr;     ///< SSM group socket address
static uint8_t            ttl = DEF_TTL;   ///< Time-to-live
static bool               verbose = false; ///< Verbose mode?

static int
getRunPar(int                   argc,
          char** const restrict argv)
{
    bool success = true;

    ifIpAddr.s_addr = inet_addr(DEF_IFACE_IP_ADDR);

    int  ch;
    while (success && (ch = getopt(argc, argv, "i:t:v")) != -1) {
        switch (ch) {
        case 'i': {
            ifaceIpAddr = optarg;
            if (inet_pton(SSM_FAMILY, ifaceIpAddr, &ifIpAddr) != 1) {
                (void)fprintf(stderr, "Couldn't parse interface IP address "
                        "\"%s\": %s\n", ifaceIpAddr, strerror(errno));
                success = false;
            }
        }
        case 't': {
            int nbytes;
            if (1 != sscanf(optarg, "%"SCNu8, &ttl)) {
                (void)fprintf(stderr,
                        "Couldn't decode time-to-live option argument \"%s\"\n",
                        optarg);
                success = false;
            }
            break;
        }
        case 'v': {
            verbose = true;
            break;
        }
        default:
            success = false;
            break;
        }
    }

    if (success) {
        success = false;

        if (optind < argc) {
            (void)fprintf(stderr, "Too many operands\n");
        }
        else {
            // Set SSM group socket address
            ssmSockAddr.sin_family = SSM_FAMILY;
            ssmSockAddr.sin_port = htons(SSM_PORT);

            if (inet_pton(SSM_FAMILY, SSM_IP_ADDR, &ssmSockAddr.sin_addr.s_addr)
                    != 1) {
                (void)fprintf(stderr, "Couldn't parse SSM IP address "
                        "\"%s\": %s\n", SSM_IP_ADDR, strerror(errno));
            }
            else {
                success = true;
            }
        }
    }

    return success;
}

static void
usage(const char* const progname)
{
    (void)fprintf(stderr,
"Usage:\n"
"    %s [-i <iface>] [-t <ttl>] [-v]\n"
"where:\n"
"    -i <iface>   IPv4 address of interface to use. Default is \"%s\".\n"
"    -t <ttl>     Time-to-live for outgoing packets. Default is %u.\n"
"    -v           Verbose output\n",
    progname, DEF_IFACE_IP_ADDR, DEF_TTL);
}

int
main(int argc, char *argv[])
{
    if (!getRunPar(argc, argv)) {
        usage(basename(argv[0]));
        exit(1);
    }

    // Create a UDP socket
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) {
        perror("socket()");
        exit(1);
    }

    // Set the interface to use for sending packets
    if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &ifIpAddr,
            sizeof(ifIpAddr))) {
        (void)fprintf(stderr, "Couldn't set sending interface to \"%s\": %s\n",
                ifaceIpAddr, strerror(errno));
        exit(1);
    }

    // Set the time-to-live of the packets
    if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))) {
        (void)fprintf(stderr, "Couldn't set time-to-live for multicast packets "
                "to %u: %s\n", ttl, strerror(errno));
        exit(1);
    }

    // Enable loopback of multicast datagrams
    {
        unsigned char yes = 1;
        if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &yes,
                sizeof(yes))) {
            perror("Couldn't enable loopback of multicast datagrams");
            exit(1);
        }
    }

    if (connect(sd, (struct sockaddr*)&ssmSockAddr, sizeof(ssmSockAddr))) {
        (void)fprintf(stderr, "Couldn't connect() to SSM group "
                "\"%s\": %s\n", SSM_SOCK_ADDR, strerror(errno));
        exit(1);
    }
    if (verbose)
        (void)printf("Multicasting to SSM group \"%s\" using interface \"%s\"\n",
                SSM_SOCK_ADDR, ifaceIpAddr);

    // Enter a sending loop
    char msg[80] = {};
    for (uint32_t i = 0; ; ++i) {
        (void)snprintf(msg, sizeof(msg), "%"PRIu32, i);
        if (send(sd, msg, strlen(msg), 0) < 0) {
             perror("send()");
             exit(1);
        }
        sleep(1);
    }
}
