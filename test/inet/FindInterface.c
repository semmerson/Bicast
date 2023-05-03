/**
 * @file FindInterface.c
 * Find the interface used for a remote address.
 *
 *  Created on: Apr 23, 2023
 *      Author: Steven R. Emmerson
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 5000

int main() {
    int                       sockfd;
    struct sockaddr_in server_addr = {};

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket() failed");
        exit(1);
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(38800);
    server_addr.sin_addr.s_addr = inet_addr("192.43.217.185"); // ldm7.frgp.net

    // Connect socket to server's address
    if (connect(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        exit(1);
    }

    // Get local socket address
    struct sockaddr_in local_addr = {};
    socklen_t                len = sizeof(local_addr);
    if (getsockname(sockfd, (struct sockaddr*)&local_addr, &len)) {
        perror("getsockname() failed");
        exit(1);
    }

    // Print the address of the local interface that would be used
    char buf[80];
    printf("Interface=%s\n", inet_ntop(local_addr.sin_family, &local_addr.sin_addr.s_addr, buf,
            sizeof(buf))); // 192.168.58.130

    // Close socket
    close(sockfd);

    return 0;
}
