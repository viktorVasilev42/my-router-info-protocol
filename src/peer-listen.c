#include "first.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

int life_table_contains_gateway(RouterState *router_state, uint8_t *gateway_to_find) {
    for (uint32_t i = 0; i < router_state->life_entries; i++) {
        if (match_ips(router_state->life_table[i].gateway, gateway_to_find)) {
            return 1;
        }
    }

    return 0;
}

int add_new_gateway_to_life_table(RouterState *router_state, uint8_t *arg_gateway) {
    if (router_state->life_entries >= ROUTER_TABLE_MAX_SIZE) {
        return -1;
    }

    memcpy(router_state->life_table[router_state->life_entries].gateway, arg_gateway, 4);
    router_state->life_table[router_state->life_entries].life_left = MAX_GATEWAY_LIFE;
    router_state->life_entries += 1;
    return 0;
}

int reset_gateway_in_life_table(RouterState *router_state, uint8_t *arg_gateway) {
    for (uint32_t i = 0; i < router_state->life_entries; i++) {
        if (match_ips(router_state->life_table[i].gateway, arg_gateway)) {
            router_state->life_table[i].life_left = MAX_GATEWAY_LIFE;
            return 1;
        }
    }

    return 0;
}

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

/* meant to be used to insert an entry for a network that is
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
            if (i == router_state->num_entries - 1) {
                memset(&router_state->router_table[i], 0, sizeof(RouterTableEntry));
            } else {
                printf("i: %u\n", i);
                printf("num_entries - 1: %u\n", router_state->num_entries - 1);
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

int set_metric_for_all_entries_with_destination(RouterState *router_state, uint8_t *arg_destination, int new_metric) {
    if (router_state->num_entries == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < router_state->num_entries; i++) {
        if (match_ips(router_state->router_table[i].destination, arg_destination)) {
            router_state->router_table[i].metric = new_metric;
        }
    }

    return 0;
}

void print_router_table(RouterState *router_state) {
    char interface_str[16];
    snprintf(interface_str, sizeof(interface_str), "%u.%u.%u.%u",
            router_state->interface_ip[0],
            router_state->interface_ip[1],
            router_state->interface_ip[2],
            router_state->interface_ip[3]
    );
    log_printf("Router Table for %s\n", interface_str);
    log_printf("%-20s %-20s %-20s %-20s\n", "Dest", "Netmask", "Gateway", "Metric");
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
        log_printf("%-20s %-20s %-20s %-20u\n",
                dest_str,
                netmask_str,
                gateway_str,
                router_state->router_table[i].metric
        );
    }
}

void print_life_table(RouterState *router_state) {
    char interface_str[16];
    snprintf(interface_str, sizeof(interface_str), "%u.%u.%u.%u",
            router_state->interface_ip[0],
            router_state->interface_ip[1],
            router_state->interface_ip[2],
            router_state->interface_ip[3]
    );
    log_printf("Life Table for %s\n", interface_str);
    log_printf("%-20s %-20s\n", "Gateway", "Life");
    for (uint32_t i = 0; i < router_state->life_entries; i++) {
        char gateway_str[16];
        snprintf(gateway_str, sizeof(gateway_str), "%u.%u.%u.%u",
                router_state->life_table[i].gateway[0],
                router_state->life_table[i].gateway[1],
                router_state->life_table[i].gateway[2],
                router_state->life_table[i].gateway[3]
        );
        log_printf("%-20s %-20u\n",
                gateway_str,
                router_state->life_table[i].life_left
        );
    }
}

void read_riptbl_and_add_to_state(int router_id, RouterState *router_state) {
    char id_str[30];
    snprintf(id_str, sizeof(id_str), "%d", router_id);
    char filename[100] = "router_";
    strncat(filename, id_str, sizeof(filename) - strlen(filename) - 1);
    strncat(filename, ".riptbl", sizeof(filename) - strlen(filename) - 1);

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("riptbl file reading error");
        free(router_state->router_table);
        free(router_state->life_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    const uint32_t MAX_LINE = 100;
    char line[MAX_LINE];
    if (fgets(line, sizeof(line), file)) {
        if (line[strlen(line) - 1] != '\n' && !feof(file)) {
            errno = EIO;
            perror("Invalid riptbl file");
            free(router_state->router_table);
            free(router_state->life_table);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        char first_line_ip[20];
        char first_line_netmask[20];
        sscanf(line, "%s %s", first_line_ip, first_line_netmask);
        if (!is_valid_ip(first_line_ip) || !is_valid_ip(first_line_netmask)) {
            errno = EIO;
            perror("Invalid riptbl file");
            free(router_state->router_table);
            free(router_state->life_table);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        inet_pton(AF_INET, first_line_ip, router_state->interface_ip);
        inet_pton(AF_INET, first_line_netmask, router_state->interface_netmask);
    }
    while (fgets(line, sizeof(line), file)) {
        if (line[strlen(line) - 1] != '\n' && !feof(file)) {
            errno = EIO;
            perror("Invalid riptbl file");
            free(router_state->router_table);
            free(router_state->life_table);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        char dest_ip[20], netmask[20], next_hop[20];
        int metric;
        if (!(sscanf(line,
                     " %19[^,], %19[^,], %19[^,], %d"
                     , dest_ip, netmask, next_hop, &metric
                    ) == 4) ||
            !is_valid_ip(dest_ip) ||
            !is_valid_ip(netmask) ||
            !is_valid_ip(next_hop)
        ) {
            errno = EIO;
            perror("Invalid riptbl file");
            free(router_state->router_table);
            free(router_state->life_table);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        add_to_table_strings(router_state, dest_ip, netmask, next_hop, metric);

        uint8_t dest_ip_uint[4];
        inet_pton(AF_INET, dest_ip, &dest_ip_uint);
        add_new_gateway_to_life_table(router_state, dest_ip_uint);
    }
    fclose(file);
}

RouterState* startup_router(uint32_t router_id) {
    RouterState *router_state = malloc(sizeof(RouterState));
    router_state->router_table = malloc(ROUTER_TABLE_MAX_SIZE * sizeof(RouterTableEntry));

    // life table def
    router_state->life_table = malloc(ROUTER_TABLE_MAX_SIZE * sizeof(LifeTableEntry));
    router_state->life_entries = 0;

    // router table and additional router state def
    router_state->num_entries = 0;
    router_state->should_restart = 0;
    router_state->should_terminate = 0;
    router_state->router_id = router_id;

    enable_logging = 1;
    uint32_t new_rand_delay = rand() % 8;
    router_state->rand_delay = new_rand_delay;
    log_printf("new_rand_delay: %u\n", 3 + new_rand_delay);

    pthread_mutex_init(&router_state->change_router_table_mutex, NULL);

    read_riptbl_and_add_to_state(router_id, router_state);
    add_to_table(
            router_state,
            router_state->interface_ip,
            router_state->interface_netmask,
            router_state->interface_ip,
            0
    );
    add_new_gateway_to_life_table(router_state, router_state->interface_ip);

    log_printf("INITIAL_STATE:\n");
    print_router_table(router_state);
    log_printf("\n");

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
        free(router_state->life_table);
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
        free(router_state->life_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        exit(EXIT_FAILURE);
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);

    uint8_t broadcast_ip[4];
    get_broadcast_ip(
        router_state->interface_ip,
        router_state->interface_netmask,
        broadcast_ip
    );
    memcpy(&broadcast_addr.sin_addr.s_addr, broadcast_ip, 4);


    while (!router_state->should_restart && !router_state->should_terminate) {
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
            free(router_state->life_table);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            exit(EXIT_FAILURE);
        }
        free(packet_to_send);

        log_printf("Broadcast message sent\n");
        print_router_table(router_state);
        // log_printf("\n");
        // print_life_table(router_state);
        log_printf("\n");
        sleep(3 + router_state->rand_delay);
    }

    close(sock);
    log_printf("rip_broadcaster ended\n");
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
        free(router_state->router_table);
        free(router_state->life_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
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
        free(router_state->life_table);
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
        free(router_state->life_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        exit(EXIT_FAILURE);
    }

    while (!router_state->should_restart && !router_state->should_terminate) {
        log_printf("Router %u.%u.%u.%u listening for broadcasts on port %d...\n",
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
            free(router_state->life_table);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            exit(EXIT_FAILURE);
        }

        if (router_state->should_restart || router_state->should_terminate) {
            close(sock);
            log_printf("rip_listen ended\n");
            return NULL;
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

        log_printf("Router %u.%u.%u.%u received on listen\n",
                router_state->interface_ip[0],
                router_state->interface_ip[1],
                router_state->interface_ip[2],
                router_state->interface_ip[3]);
        // print_router_table(rec_router_state);

        pthread_mutex_lock(&router_state->change_router_table_mutex);
        for (uint32_t i = 0; i < rec_router_state->num_entries; i++) {
            if (match_ips(rec_router_state->router_table[i].gateway,
                          router_state->interface_ip)) {
                continue;
            }

            int index_of_exact_dest = find_index_of_network_that_exacts(
                        router_state,
                        rec_router_state->router_table[i].destination,
                        rec_router_state->router_table[i].netmask
            );

            if (index_of_exact_dest != -1) {
                // a network in my router table is exactly the currenty received network
                uint32_t old_metric = router_state->
                    router_table[index_of_exact_dest].metric;
                uint32_t rec_metric = rec_router_state->
                    router_table[i].metric;
                if (match_ips(
                        router_state->router_table[index_of_exact_dest].gateway,
                        rec_router_state->interface_ip) &&
                    rec_router_state->router_table[i].metric != 0) {
                    // the gateway for this entry is the router i currently receive from,
                    // so i trust the received metric and update even if it is worse
                    router_state->router_table[index_of_exact_dest].metric =
                        cap_metric(rec_metric);

                } else if (rec_metric + 1 < old_metric) {
                    // the gateway for this entry is being changed, so i have to check if
                    // the new metric is better than the old one before updating
                    memcpy(router_state->router_table[index_of_exact_dest].gateway,
                            rec_router_state->interface_ip,
                            4
                    );
                    router_state->router_table[index_of_exact_dest].metric =
                        cap_metric(rec_metric + 1);
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
                            cap_metric(rec_router_state->router_table[i].metric + 1)
                    );
                } else {
                    // this is the first time i encounter this netowork
                    // just add it to the table
                    add_to_table(
                            router_state,
                            rec_router_state->router_table[i].destination,
                            rec_router_state->router_table[i].netmask,
                            rec_router_state->interface_ip,
                            cap_metric(rec_router_state->router_table[i].metric + 1)
                    );
                }
            }

            if (!life_table_contains_gateway(router_state, rec_router_state->router_table[i].destination)) {
                add_new_gateway_to_life_table(router_state, rec_router_state->router_table[i].destination);
            } else {
                reset_gateway_in_life_table(router_state, rec_router_state->router_table[i].destination);
            }
        }
        pthread_mutex_unlock(&router_state->change_router_table_mutex);

        free(rec_router_state->router_table);
        free(rec_router_state->life_table);
        free(rec_router_state);
    }

    close(sock);
    log_printf("rip_listen ended\n");
    return NULL;
}

void send_tombstone_packet(RouterState *router_state) {
    int sock;
    struct sockaddr_in tombstone_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        free(router_state->router_table);
        free(router_state->life_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        exit(EXIT_FAILURE);
    }

    tombstone_addr.sin_family = AF_INET;
    tombstone_addr.sin_port = htons(BROADCAST_PORT);
    inet_pton(AF_INET, "127.0.0.1", &tombstone_addr.sin_addr.s_addr);

    const uint32_t SIZEOF_PACKET_TO_SEND = 1;
    const uint8_t data_to_send = 1;
    ssize_t sendto_res =
        sendto(sock, &data_to_send, SIZEOF_PACKET_TO_SEND, 0,
               (struct sockaddr *)&tombstone_addr, sizeof(tombstone_addr));
    if (sendto_res < 0) {
        perror("sendto failed");
        free(router_state->router_table);
        free(router_state->life_table);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        close(sock);
        exit(EXIT_FAILURE);
    }

    close(sock);
    log_printf("Tombstone packet sent\n");
}

HandleCmdReturnCode handle_cmd(char *cmd, RouterState *router_state) {
    if (strcmp(cmd, "log on") == 0) {
        printf("Logging on.\n");
        enable_logging = 1;
        return CMD_DONT_RESTORE_SHOULD_LOG;
    }
    else if (strcmp(cmd, "log off") == 0) {
        printf("Logging off.\n");
        enable_logging = 0;
        return CMD_DONT_RESTORE_SHOULD_LOG;
    }
    else if (strcmp(cmd, "reload") == 0) {
        printf("Reloading router...\n");
        router_state->should_restart = 1;
        send_tombstone_packet(router_state);
        return CMD_RESTART_ROUTER;
    }
    else if (strcmp(cmd, "exit") == 0) {
        printf("Terminating router...\n");
        router_state->should_terminate = 1;
        send_tombstone_packet(router_state);
        return CMD_TERMINATE;
    }

    return CMD_DONE;
}

void* listen_for_command(void *arg_router_state) {
    RouterState *router_state = (RouterState*) arg_router_state;
    log_printf("Press '+' to stop logging...\n");

    HandleCmdReturnCode last_cmd_return_code = CMD_DONE;

    while (last_cmd_return_code != CMD_RESTART_ROUTER &&
           last_cmd_return_code != CMD_TERMINATE) {
        enable_raw_term();
        char c = getchar();
        if (c == '+') {
            if (enable_logging) {
                log_printf("Logging stopped.\n");
                enable_logging = 0;
            } else {
                enable_logging = 1;
                log_printf("Logging started.\n");
            }
        }
        else if (c == '/') {
            int old_should_log = enable_logging;
            enable_logging = 0;
            printf("/");
            disable_raw_term();

            const size_t cmd_size = 100;
            char cmd_buffer[cmd_size];
            fgets(cmd_buffer, cmd_size, stdin);
            cmd_buffer[strcspn(cmd_buffer, "\n")] = '\0';
            last_cmd_return_code = handle_cmd(cmd_buffer, router_state);

            if (last_cmd_return_code != CMD_DONT_RESTORE_SHOULD_LOG) {
                enable_logging = old_should_log;
            }
        }
    }

    log_printf("listen_for_command ended\n");
    return NULL;
}

void* gateway_life_clock(void *arg_router_state) {
    RouterState *router_state = (RouterState*) arg_router_state;

    while (!router_state->should_restart && !router_state->should_terminate) {
      sleep(TIME_FOR_LIFE_DROP);
      for (uint32_t i = 0; i < router_state->life_entries; i++) {
          if (router_state->life_table[i].life_left == 0) {
              set_metric_for_all_entries_with_destination(
                      router_state,
                      router_state->life_table[i].gateway,
                      INFINITY_METRIC
              );
          } else if (!match_ips(router_state->life_table[i].gateway,
                                router_state->interface_ip)) {
              router_state->life_table[i].life_left -= 1;
          }
      }
    }

    log_printf("gateway_life_clock ended\n");
    return NULL;
}

void split_threads(RouterState *router_state) {
    int is_thread_error = 0;
    uint32_t the_router_id = router_state->router_id;
    pthread_t threads[4];
    int rc_one = pthread_create(&threads[0], NULL, rip_listen, (void*) router_state);
    if (rc_one) {
        perror("Error initializing threads.");
        is_thread_error = 1;
        goto cleanup_router_state;
    }

    sleep(1);
    int rc_two = pthread_create(&threads[1], NULL, rip_broadcaster, (void*) router_state);
    if (rc_two) {
        perror("Error initializing threads.");
        is_thread_error = 1;
        goto cleanup_router_state;
    }

    sleep(1);
    int rc_three = pthread_create(&threads[2], NULL, listen_for_command, (void*) router_state);
    if (rc_three) {
        perror("Error initializing threads.");
        is_thread_error = 1;
        goto cleanup_router_state;
    }

    int rc_four = pthread_create(&threads[3], NULL, gateway_life_clock, (void*) router_state);
    if (rc_four) {
        perror("Error initializing threads.");
        is_thread_error = 1;
        goto cleanup_router_state;
    }

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    pthread_join(threads[2], NULL);
    pthread_join(threads[3], NULL);

    int was_should_terminate = router_state->should_terminate;

cleanup_router_state:
    free(router_state->router_table);
    free(router_state->life_table);
    pthread_mutex_destroy(&router_state->change_router_table_mutex);
    free(router_state);

    if (is_thread_error) {
        // there was a thread initialization error
        exit(EXIT_FAILURE);
    } else if (was_should_terminate) {
        // the user requested for the router to terminate
        exit(EXIT_SUCCESS);
    } else {
        // recursive call - the router should restart
        RouterState *reloaded_router = startup_router(the_router_id);
        split_threads(reloaded_router);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        exit(EXIT_FAILURE);
    }

    setbuf(stdout, NULL);
    uint32_t curr_num_router = atoi(argv[1]);

    srand(time(NULL));
    RouterState *router_one = startup_router(curr_num_router);
    split_threads(router_one);

    log_printf("main thread ended\n");
    pthread_exit(NULL);
}
