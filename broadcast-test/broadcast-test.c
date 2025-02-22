#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "first.h"

void broadcaster() {
    int sock;
    struct sockaddr_in broadcast_addr;
    char *message = "Hello from broadcaster";

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int broadcast_enable = 1;
    int setsock_res = setsockopt(
            sock,
            SOL_SOCKET, SO_BROADCAST,
            &broadcast_enable, sizeof(broadcast_enable)
    );
    if (setsock_res < 0) {
        perror("setsockopt failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    ssize_t sendto_res = sendto(
            sock, message, strlen(message), 0,
            (struct sockaddr*) &broadcast_addr,
            sizeof(broadcast_addr)
    );
    if (sendto_res < 0) {
        perror("sendto failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Broadcast message sent: %s\n", message);
    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [broadcast|listen]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "broadcast") == 0) {
        broadcaster();
    } else {
        fprintf(stderr, "Invalid argument: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

