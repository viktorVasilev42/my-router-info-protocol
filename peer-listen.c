#include "first.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    uint8_t destination[4];
    uint8_t netmask[4];
    uint8_t gateway[4];
    uint32_t metric;
} RouterTableEntry;

typedef struct {
    uint8_t interface_ip[4];
    RouterTableEntry *router_table;
    uint32_t num_entries;
    uint32_t rand_delay;
    pthread_mutex_t change_router_table_mutex;
} RouterState;

int add_to_table(RouterState *router_state,
        uint8_t *dest,
        uint8_t *netmask,
        uint8_t *gateway,
        uint32_t metric) {

    if (router_state->num_entries >= ROUTER_TABLE_MAX_SIZE) {
        return -1;
    }

    memcpy(router_state->router_table[router_state->num_entries].destination, dest, 4);
    memcpy(router_state->router_table[router_state->num_entries].netmask, netmask, 4);
    memcpy(router_state->router_table[router_state->num_entries].gateway, gateway, 4);
    router_state->router_table[router_state->num_entries].metric = metric;
    router_state->num_entries += 1;
    return 0;
}

/* meant to be use to insert an entry for a network that is
 * subsumed by another and needs to be inserted before it
 *
 * one cannot add an entry at a position that isn't already taken
 * */
int add_to_table_at_pos(RouterState *router_state, uint32_t pos,
        uint8_t *dest,
        uint8_t *netmask,
        uint8_t *gateway,
        uint32_t metric) {

    if (pos >= router_state->num_entries) {
        return -1;
    }

    if (router_state->num_entries >= ROUTER_TABLE_MAX_SIZE) {
        return -1;
    }

    for (uint32_t i = router_state->num_entries - 1; i > pos; i--) {
        memcpy(&router_state->router_table[i + 1], &router_state->router_table[i], sizeof(RouterTableEntry));
    }
    memcpy(&router_state->router_table[pos + 1], &router_state->router_table[pos], sizeof(RouterTableEntry));

    memcpy(router_state->router_table[pos].destination, dest, 4);
    memcpy(router_state->router_table[pos].netmask, netmask, 4);
    memcpy(router_state->router_table[pos].gateway, gateway, 4);
    router_state->router_table[pos].metric = metric;
    router_state->num_entries += 1;
    return 0;
}

int add_to_table_strings(RouterState *router_state,
        const char *dest,
        const char *netmask,
        const char *gateway,
        uint32_t metric) {

    if (router_state->num_entries >= ROUTER_TABLE_MAX_SIZE) {
        return -1;
    }

    int dest_rc = inet_pton(AF_INET, dest, router_state->router_table[router_state->num_entries].destination);
    int mask_rc = inet_pton(AF_INET, netmask, router_state->router_table[router_state->num_entries].netmask);
    int gateway_rc = inet_pton(AF_INET, gateway, router_state->router_table[router_state->num_entries].gateway);
    router_state->router_table[router_state->num_entries].metric = metric;

    if (dest_rc != 1 || mask_rc != 1 || gateway_rc != 1) {
        return -1;
    }

    router_state->num_entries += 1;
    return 0;
}

int add_to_table_at_pos_strings(RouterState *router_state, uint32_t pos,
        const char *dest,
        const char *netmask,
        const char *gateway,
        uint32_t metric) {

    uint8_t arg_dest[4];
    uint8_t arg_netmask[4];
    uint8_t arg_gateway[4];

    int dest_rc = inet_pton(AF_INET, dest, arg_dest);
    int mask_rc = inet_pton(AF_INET, netmask, arg_netmask);
    int gateway_rc = inet_pton(AF_INET, gateway, arg_gateway);

    if (dest_rc != 1 || mask_rc != 1 || gateway_rc != 1) {
        return -1;
    }

    return add_to_table_at_pos(router_state, pos, arg_dest, arg_netmask, arg_gateway, metric);
}

int remove_from_table_for_dest(RouterState *router_state, const char *dest) {
    if (router_state->num_entries == 0) {
        return -1;
    }

    uint8_t proper_dest[4];
    int dest_rc = inet_pton(AF_INET, dest, proper_dest);
    if (dest_rc != 1) {
        return -1;
    }

    for (uint32_t i = 0; i < router_state->num_entries; i++) {
        if (match_ips(router_state->router_table[i].destination, proper_dest)) {
            if (router_state->num_entries == 1) {
                // i is 0 here definitely
                memset(&router_state->router_table[i], 0, sizeof(RouterTableEntry));
            } else {
                memcpy(&router_state->router_table[i],
                       &router_state->router_table[router_state->num_entries - 1],
                       sizeof(RouterTableEntry)
                );
                memset(&router_state->router_table[router_state->num_entries - 1], 0, sizeof(RouterTableEntry));
            }

            router_state->num_entries -= 1;
            return 0;
        }
    }

    return -1;
}

int is_network_subsumed(uint8_t *ip1, uint8_t *mask1, uint8_t *ip2, uint8_t *mask2) {
    int mask1_bits = 0, mask2_bits = 0;
    for (int i = 0; i < 4; i++) {
        mask1_bits += __builtin_popcount(mask1[i]);
        mask2_bits += __builtin_popcount(mask2[i]);
    }

    if (mask1_bits > mask2_bits) return 0;

    for (int i = 0; i < 4; i++) {
        if ((ip1[i] & mask1[i]) != (ip2[i] & mask1[i])) {
            return 0;
        }
    }

    return 1;
}

void print_router_table(RouterState *router_state) {
    char interface_str[16];
    snprintf(interface_str, sizeof(interface_str), "%u.%u.%u.%u",
            router_state->interface_ip[0],
            router_state->interface_ip[1],
            router_state->interface_ip[2],
            router_state->interface_ip[3]);
    printf("Router Table for %s\n", interface_str);
    printf("%-20s %-20s %-20s %-20s\n", "Dest", "Netmask", "Gateway", "Metric");
    for (uint32_t i = 0; i < router_state->num_entries; i++) {
        char dest_str[16];
        char netmask_str[16];
        char gateway_str[16];
        snprintf(dest_str, sizeof(dest_str), "%u.%u.%u.%u",
                router_state->router_table[i].destination[0],
                router_state->router_table[i].destination[1],
                router_state->router_table[i].destination[2],
                router_state->router_table[i].destination[3]
        );
        snprintf(netmask_str, sizeof(netmask_str), "%u.%u.%u.%u",
                router_state->router_table[i].netmask[0],
                router_state->router_table[i].netmask[1],
                router_state->router_table[i].netmask[2],
                router_state->router_table[i].netmask[3]
        );
        snprintf(gateway_str, sizeof(gateway_str), "%u.%u.%u.%u",
                router_state->router_table[i].gateway[0],
                router_state->router_table[i].gateway[1],
                router_state->router_table[i].gateway[2],
                router_state->router_table[i].gateway[3]
        );
        printf("%-20s %-20s %-20s %-20u\n",
                dest_str,
                netmask_str,
                gateway_str,
                router_state->router_table[i].metric
        );
    }
}

RouterState* startup_router(int router_id) {
    RouterState *router_state = malloc(sizeof(RouterState));
    router_state->router_table = malloc(ROUTER_TABLE_MAX_SIZE * sizeof(RouterTableEntry));
    router_state->num_entries = 0;

    uint32_t new_rand_delay = rand() % 8;
    router_state->rand_delay = new_rand_delay;
    printf("new_rand_delay: %u\n", 3 + new_rand_delay);

    pthread_mutex_init(&router_state->change_router_table_mutex, NULL);

    int interface_rc = 0;
    if (router_id == 1) {
        interface_rc = inet_pton(AF_INET, "10.0.0.1", router_state->interface_ip);

        add_to_table_strings(router_state, "3.4.5.6", "255.255.255.0", "10.0.0.11", 12);
        add_to_table_strings(router_state, "192.168.1.128", "255.255.255.128", "10.0.0.9", 5);
    } else if (router_id == 2) {
        interface_rc = inet_pton(AF_INET, "10.0.0.2", router_state->interface_ip);

        add_to_table_strings(router_state, "192.168.1.194", "255.255.255.192", "10.0.0.4", 7);
        add_to_table_strings(router_state, "192.168.1.128", "255.255.255.128", "10.0.0.7", 3);
    }

    if (interface_rc != 1) {
        exit(EXIT_FAILURE);
    }
    printf("INITIAL_STATE:\n");
    print_router_table(router_state);
    printf("\n");

    return router_state;
}

int find_index_of_network_that_exacts(RouterState *router_state, uint8_t *ip_to_find, uint8_t *mask_to_find) {
    for (int i = 0; i < router_state->num_entries; i++) {
        if (match_ips(router_state->router_table[i].destination, ip_to_find) &&
                match_ips(router_state->router_table[i].netmask, mask_to_find)
        ) {
            return i;
        }
    }

    return -1;
}

int find_index_of_network_that_subsumes(RouterState *router_state, uint8_t *ip_to_find, uint8_t *mask_to_find) {
    for (int i = 0; i < router_state->num_entries; i++) {
        if (is_network_subsumed(
                    router_state->router_table[i].destination,
                    router_state->router_table[i].netmask,
                    ip_to_find,
                    mask_to_find)
        ) {
            return i;
        }
    }

    return -1;
}

void* rip_broadcaster(void *arg_router_state) {
    RouterState *router_state = (RouterState*) arg_router_state;

    int sock;
    struct sockaddr_in broadcast_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        free(router_state->router_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
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
        free(router_state->router_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        exit(EXIT_FAILURE);
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");


    while (1) {
        pthread_mutex_lock(&router_state->change_router_table_mutex);
        const uint32_t SIZEOF_PACKET_TO_SEND =
            (router_state->num_entries * sizeof(RouterTableEntry)) + 8;

        // plus 8 bytes because we send the interface_ip and num_entries
        uint8_t *packet_to_send = malloc(SIZEOF_PACKET_TO_SEND);

        memcpy(packet_to_send, router_state->interface_ip, 4);
        memcpy(packet_to_send + 4, &router_state->num_entries, 4);
        memcpy(packet_to_send + 8, router_state->router_table,
               router_state->num_entries * sizeof(RouterTableEntry));
        pthread_mutex_unlock(&router_state->change_router_table_mutex);

        ssize_t sendto_res =
            sendto(sock, packet_to_send, SIZEOF_PACKET_TO_SEND, 0,
                   (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
        if (sendto_res < 0) {
            perror("sendto failed");
            free(packet_to_send);
            close(sock);
            free(router_state->router_table);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            exit(EXIT_FAILURE);
        }

        printf("Broadcast message sent\n");
        sleep(3 + router_state->rand_delay);
        free(packet_to_send);
    }

    close(sock);
    free(router_state->router_table);
    pthread_mutex_destroy(&router_state->change_router_table_mutex);
    free(router_state);
    printf("rip_broadcaster ended\n");
    exit(EXIT_FAILURE);
    return NULL;
}


void* rip_listen(void *arg_router_state) {
    RouterState *router_state = (RouterState*) arg_router_state;

    int sock;
    struct sockaddr_in listen_addr, sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    uint8_t rec_buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    int setsock_res = setsockopt(sock,
            SOL_SOCKET, SO_REUSEADDR,
            &reuse, sizeof(reuse)
    );
    if (setsock_res < 0) {
        perror("setsockopt with SO_REUSEADDR failed");
        close(sock);
        free(router_state->router_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        exit(EXIT_FAILURE);
    }

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(BROADCAST_PORT);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bind_res = bind(sock,
            (struct sockaddr*) &listen_addr,
            sizeof(listen_addr)
    );
    if (bind_res < 0) {
        perror("bind failed");
        close(sock);
        free(router_state->router_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("Router %u.%u.%u.%u listening for broadcasts on port %d...\n",
                router_state->interface_ip[0],
                router_state->interface_ip[1],
                router_state->interface_ip[2],
                router_state->interface_ip[3],
                BROADCAST_PORT);

        int bytes_received = recvfrom(sock,
                rec_buffer, BUFFER_SIZE - 1,
                0,
                (struct sockaddr*) &sender_addr,
                &addr_len
        );
        if (bytes_received < 0) {
            perror("recvform failed");
            close(sock);
            free(router_state->router_table);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            exit(EXIT_FAILURE);
        }

        RouterState *rec_router_state = malloc(sizeof(RouterState));
        rec_router_state->router_table = malloc(ROUTER_TABLE_MAX_SIZE * sizeof(RouterTableEntry));
        memcpy(rec_router_state->interface_ip, rec_buffer, 4);

        if (match_ips(rec_router_state->interface_ip, router_state->interface_ip)) {
            free(rec_router_state->router_table);
            free(rec_router_state);
            continue;
        }
        memcpy(&rec_router_state->num_entries, rec_buffer + 4, 4);
        memcpy(rec_router_state->router_table,
                rec_buffer + 8,
                rec_router_state->num_entries * sizeof(RouterTableEntry));

        printf("Router %u.%u.%u.%u received on listen\n",
                router_state->interface_ip[0],
                router_state->interface_ip[1],
                router_state->interface_ip[2],
                router_state->interface_ip[3]);
        print_router_table(rec_router_state);

        pthread_mutex_lock(&router_state->change_router_table_mutex);
        for (uint32_t i = 0; i < rec_router_state->num_entries; i++) {
            int index_of_exact_dest = find_index_of_network_that_exacts(
                        router_state,
                        rec_router_state->router_table[i].destination,
                        rec_router_state->router_table[i].netmask
            );

            if (index_of_exact_dest != -1) {
                // a network in my router table is exactly the currenty received network
                uint32_t old_metric = router_state->
                    router_table[index_of_exact_dest].metric;
                uint32_t new_metric = rec_router_state->
                    router_table[i].metric;
                if (new_metric + 1 < old_metric) {
                    memcpy(router_state->router_table[index_of_exact_dest].gateway,
                            rec_router_state->interface_ip,
                            4
                    );
                    router_state->router_table[index_of_exact_dest].metric =
                        rec_router_state->router_table[i].metric + 1;
                }
            } else {
                int index_of_parent_network = find_index_of_network_that_subsumes(
                        router_state,
                        rec_router_state->router_table[i].destination,
                        rec_router_state->router_table[i].netmask
                );

                if (index_of_parent_network != -1) {
                    // a network in my router table subsumes the currently received network
                    // add the new network before the parent network in the router table
                    add_to_table_at_pos(
                            router_state, index_of_parent_network,
                            rec_router_state->router_table[i].destination,
                            rec_router_state->router_table[i].netmask,
                            rec_router_state->interface_ip,
                            rec_router_state->router_table[i].metric + 1
                    );
                } else {
                    // this is the first time i encounter this netowork
                    // just add it to the table
                    add_to_table(
                            router_state,
                            rec_router_state->router_table[i].destination,
                            rec_router_state->router_table[i].netmask,
                            rec_router_state->interface_ip,
                            rec_router_state->router_table[i].metric + 1
                    );
                }
            }
        }
        pthread_mutex_unlock(&router_state->change_router_table_mutex);

        free(rec_router_state->router_table);
        free(rec_router_state);
    }

    close(sock);
    free(router_state->router_table);
    pthread_mutex_destroy(&router_state->change_router_table_mutex);
    free(router_state);
    printf("rip_listen ended\n");
    exit(EXIT_FAILURE);
    return NULL;
}

void split_threads(RouterState *router_state) {
    pthread_t threads[2];
    int rc_one = pthread_create(&threads[0], NULL, rip_listen, (void*) router_state);
    if (rc_one) {
        printf("Error");
        exit(EXIT_FAILURE);
    }

    sleep(1);
    int rc_two = pthread_create(&threads[1], NULL, rip_broadcaster, (void*) router_state);
    if (rc_two) {
        printf("Error");
        exit(EXIT_FAILURE);
    }
}

int main() {
    srand(time(NULL));
    RouterState *router_one = startup_router(1);
    RouterState *router_two = startup_router(2);

    split_threads(router_one);
    split_threads(router_two);

    pthread_exit(NULL);
}
