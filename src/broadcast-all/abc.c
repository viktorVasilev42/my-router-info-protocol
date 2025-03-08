#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BROADCAST_PORT 12345
#define BROADCAST_ADDR "255.255.255.255"

typedef struct {
    uint8_t data[4];  // Array of 4 uint8_t
    uint32_t number;  // A uint32_t number
} Message;

void *broadcast_thread(void *arg) {
    int sock;
    struct sockaddr_in broadcast_addr;
    Message message = {{1, 2, 3, 4}, 422554};  // Example message with 4 elements in the array and number 42

    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Enable broadcasting
    int broadcast_permission = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_permission, sizeof(broadcast_permission)) < 0) {
        perror("Error setting broadcast option");
        close(sock);
        return NULL;
    }

    // Setup broadcast address
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);

    // Send broadcast
    while (1) {
        if (sendto(sock, &message, sizeof(message), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            perror("Broadcast send failed");
            close(sock);
            return NULL;
        }
        printf("Broadcasted: {data: {%u, %u, %u, %u}, number: %u}\n", message.data[0], message.data[1], message.data[2], message.data[3], message.number);
        sleep(1);  // Send every second
    }

    close(sock);
    return NULL;
}

void *listen_thread(void *arg) {
    int sock;
    struct sockaddr_in listen_addr;
    Message received_message;
    socklen_t addr_len = sizeof(listen_addr);

    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Setup listen address
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(BROADCAST_PORT);
    listen_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return NULL;
    }

    // Listen for incoming messages
    while (1) {
        if (recvfrom(sock, &received_message, sizeof(received_message), 0, (struct sockaddr *)&listen_addr, &addr_len) < 0) {
            perror("Receive failed");
            close(sock);
            return NULL;
        }
        printf("Received: {data: {%u, %u, %u, %u}, number: %u}\n", received_message.data[0], received_message.data[1], received_message.data[2], received_message.data[3], received_message.number);
    }

    close(sock);
    return NULL;
}

int main() {
    pthread_t broadcast_tid, listen_tid;

    // Create the broadcasting thread
    if (pthread_create(&broadcast_tid, NULL, broadcast_thread, NULL) != 0) {
        perror("Failed to create broadcast thread");
        return EXIT_FAILURE;
    }

    // Create the listening thread
    if (pthread_create(&listen_tid, NULL, listen_thread, NULL) != 0) {
        perror("Failed to create listen thread");
        return EXIT_FAILURE;
    }

    // Wait for threads to finish (they won't, since they are infinite loops)
    pthread_join(broadcast_tid, NULL);
    pthread_join(listen_tid, NULL);

    return 0;
}

