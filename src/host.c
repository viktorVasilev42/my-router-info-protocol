#include "host.h"
#include <errno.h>
#include "first.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

const uint32_t HOST_RAND_DELAY_BONUS = 3;

void* host_broadcaster(void *arg_host_state) {
    HostState *host_state = (HostState*) arg_host_state;

    int sock;
    struct sockaddr_in broadcast_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        free(host_state);
        exit(EXIT_FAILURE);
    }

    int broadcast_enable = 1;
    int setsockopt_res = setsockopt(
            sock,
            SOL_SOCKET, SO_BROADCAST,
            &broadcast_enable, sizeof(broadcast_enable)
    );
    if (setsockopt_res < 0) {
        perror("setsockopt failed");
        close(sock);
        free(host_state);
        exit(EXIT_FAILURE);
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);

    uint8_t broadcast_ip[4];
    get_broadcast_ip(
        host_state->interface_ip,
        host_state->interface_netmask,
        broadcast_ip
    );
    memcpy(&broadcast_addr.sin_addr.s_addr, broadcast_ip, 4);

    while (1) {
        // one table entry [ip, netmask, ip, 0]
        // plus 8 bytes - interface_ip and interface_netmask of host
        const uint32_t SIZEOF_PACKET_TO_SEND = 
            sizeof(RouterTableEntry) + 8;
        uint8_t *packet_to_send = malloc(SIZEOF_PACKET_TO_SEND);

        memcpy(packet_to_send, host_state->interface_ip, 4);
        uint32_t SIZE_OF_ONE_ENTRY = 1;
        memcpy(packet_to_send + 4, &SIZE_OF_ONE_ENTRY, 4);
        RouterTableEntry table_entry_to_send = {
            .destination = {
                host_state->interface_ip[0],
                host_state->interface_ip[1],
                host_state->interface_ip[2],
                host_state->interface_ip[3]
            },
            .netmask = {
                host_state->interface_netmask[0],
                host_state->interface_netmask[1],
                host_state->interface_netmask[2],
                host_state->interface_netmask[3]
            },
            .gateway = {
                host_state->interface_ip[0],
                host_state->interface_ip[1],
                host_state->interface_ip[2],
                host_state->interface_ip[3]
            },
            .metric = 0
        };
        memcpy(packet_to_send + 8, &table_entry_to_send, sizeof(RouterTableEntry));

        ssize_t sendto_res =
            sendto(sock, packet_to_send, SIZEOF_PACKET_TO_SEND, 0,
                    (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        if (sendto_res < 0) {
            perror("sendto failed");
            free(packet_to_send);
            close(sock);
            free(host_state);
            exit(EXIT_FAILURE);
        }

        free(packet_to_send);
        
        char interface_ip_str[16];
        snprintf(interface_ip_str, sizeof(interface_ip_str), "%u.%u.%u.%u",
                host_state->interface_ip[0],
                host_state->interface_ip[1],
                host_state->interface_ip[2],
                host_state->interface_ip[3]
        );
        log_printf("Host %s broadcast message sent\n", interface_ip_str);
        sleep(host_state->rand_delay);
    }

    close(sock);
    return NULL;
}

int read_hosttbl_and_add_to_state(int host_id, HostState *host_state) {
    char id_str[30];
    snprintf(id_str, sizeof(id_str), "%d", host_id);
    char filename[100] = "host_";
    strncat(filename, id_str, sizeof(filename) - strlen(filename) - 1);
    strncat(filename, ".hosttbl", sizeof(filename) - strlen(filename) - 1);

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("hosttbl file reading errror");
        fclose(file);
        return -1;
    }

    const uint32_t MAX_LINE = 100;
    char line[MAX_LINE];
    if (!fgets(line, sizeof(line), file)) {
        errno = EIO;
        perror("Invalid hosttbl");
        fclose(file);
        return -1;
    }

    if (line[strlen(line) - 1] != '\n') {
        errno = EIO;
        perror("Invalid hosttbl");
        fclose(file);
        return -1;
    }

    char first_line_ip[20];
    char first_line_netmask[20];
    sscanf(line, "%s %s", first_line_ip, first_line_netmask);
    if (!is_valid_ip(first_line_ip) || !is_valid_ip(first_line_netmask)) {
        errno = EIO;
        perror("Invalid hosttbl");
        fclose(file);
        return -1;
    }
    
    inet_pton(AF_INET, first_line_ip, host_state->interface_ip);
    inet_pton(AF_INET, first_line_netmask, host_state->interface_netmask);

    return 0;
}

HostState* startup_host(uint32_t host_id) {
    HostState *host_state = malloc(sizeof(HostState));

    host_state->rand_delay = (rand() % 8) + HOST_RAND_DELAY_BONUS;
    log_printf("host_rand_delay: %u\n", host_state->rand_delay);
    int read_hosttbl_rc = read_hosttbl_and_add_to_state(host_id, host_state);
    if (read_hosttbl_rc < 0) {
        free(host_state);
        exit(EXIT_FAILURE);
    }

    return host_state;
}

int host_split_threads(HostState *host_state) {
    int is_thread_error = 0;
    pthread_t threads[1];
    int rc_one = pthread_create(&threads[0], NULL, host_broadcaster, (void*) host_state);
    if (rc_one) {
        perror("Error initializing threads.");
        is_thread_error = 1;
        goto cleanup_host_state;
    }

    pthread_join(threads[0], NULL);

cleanup_host_state:
    free(host_state);

    if (is_thread_error) {
        return -1;
    }
    return 0;
}
