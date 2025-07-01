#include <first.h>
#include <host.h>
#include <errno.h>
#include <netinet/in.h>
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
        uint8_t *if_to_hop,
        uint32_t metric) {

    if (router_state->num_entries >= ROUTER_TABLE_MAX_SIZE) {
        return -1;
    }

    memcpy(router_state->router_table[router_state->num_entries].destination, dest, 4);
    memcpy(router_state->router_table[router_state->num_entries].netmask, netmask, 4);
    memcpy(router_state->router_table[router_state->num_entries].gateway, gateway, 4);
    memcpy(router_state->router_table[router_state->num_entries].interface, if_to_hop, 4);
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
        uint8_t *if_to_hop,
        uint32_t metric) {

    if (pos >= router_state->num_entries) {
        return -1;
    }

    if (router_state->num_entries >= ROUTER_TABLE_MAX_SIZE) {
        return -1;
    }

    for (uint32_t i = router_state->num_entries - 1; i >= pos; i--) {
        memcpy(&router_state->router_table[i + 1], &router_state->router_table[i], sizeof(RouterTableEntry));
    }

    memcpy(router_state->router_table[pos].destination, dest, 4);
    memcpy(router_state->router_table[pos].netmask, netmask, 4);
    memcpy(router_state->router_table[pos].gateway, gateway, 4);
    memcpy(router_state->router_table[pos].interface, if_to_hop, 4);
    router_state->router_table[pos].metric = metric;
    router_state->num_entries += 1;
    return 0;
}

int add_to_table_strings(RouterState *router_state,
        const char *dest,
        const char *netmask,
        const char *gateway,
        const char *if_to_hop,
        uint32_t metric) {

    if (router_state->num_entries >= ROUTER_TABLE_MAX_SIZE) {
        return -1;
    }

    int dest_rc = inet_pton(AF_INET, dest, router_state->router_table[router_state->num_entries].destination);
    int mask_rc = inet_pton(AF_INET, netmask, router_state->router_table[router_state->num_entries].netmask);
    int gateway_rc = inet_pton(AF_INET, gateway, router_state->router_table[router_state->num_entries].gateway);
    int if_to_hop_rc = inet_pton(AF_INET, if_to_hop, router_state->router_table[router_state->num_entries].interface);
    router_state->router_table[router_state->num_entries].metric = metric;

    if (dest_rc != 1 || mask_rc != 1 || gateway_rc != 1 || if_to_hop_rc != 1) {
        return -1;
    }

    router_state->num_entries += 1;
    return 0;
}

int add_to_table_at_pos_strings(RouterState *router_state, uint32_t pos,
        const char *dest,
        const char *netmask,
        const char *gateway,
        const char *if_to_hop,
        uint32_t metric) {

    uint8_t arg_dest[4];
    uint8_t arg_netmask[4];
    uint8_t arg_gateway[4];
    uint8_t arg_if_to_hop[4];

    int dest_rc = inet_pton(AF_INET, dest, arg_dest);
    int mask_rc = inet_pton(AF_INET, netmask, arg_netmask);
    int gateway_rc = inet_pton(AF_INET, gateway, arg_gateway);
    int if_to_hop_rc = inet_pton(AF_INET, if_to_hop, arg_if_to_hop);

    if (dest_rc != 1 || mask_rc != 1 || gateway_rc != 1 || if_to_hop_rc != 1) {
        return -1;
    }

    return add_to_table_at_pos(
        router_state,
        pos,
        arg_dest,
        arg_netmask,
        arg_gateway,
        arg_if_to_hop,
        metric
    );
}

// TODO - DELETE - Currently not being used:
//
// int remove_from_table_for_dest(RouterState *router_state, const char *dest) {
//     if (router_state->num_entries == 0) {
//         return -1;
//     }
//
//     uint8_t proper_dest[4];
//     int dest_rc = inet_pton(AF_INET, dest, proper_dest);
//     if (dest_rc != 1) {
//         return -1;
//     }
//
//     for (uint32_t i = 0; i < router_state->num_entries; i++) {
//         if (match_ips(router_state->router_table[i].destination, proper_dest)) {
//             if (i == router_state->num_entries - 1) {
//                 memset(&router_state->router_table[i], 0, sizeof(RouterTableEntry));
//             } else {
//                 memcpy(&router_state->router_table[i],
//                        &router_state->router_table[router_state->num_entries - 1],
//                        sizeof(RouterTableEntry)
//                 );
//                 memset(&router_state->router_table[router_state->num_entries - 1], 0, sizeof(RouterTableEntry));
//             }
//
//             router_state->num_entries -= 1;
//             return 0;
//         }
//     }
//
//     return -1;
// }

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
        router_state->interfaces[0].interface_ip[0],
        router_state->interfaces[0].interface_ip[1],
        router_state->interfaces[0].interface_ip[2],
        router_state->interfaces[0].interface_ip[3]
    );
    log_printf("Router Table for %s\n", interface_str);
    log_printf("%-20s %-20s %-20s %-20s\n", "Dest", "Netmask", "Gateway", "Metric");
    for (uint32_t i = 0; i < router_state->num_entries; i++) {
        char dest_str[INET_ADDRSTRLEN];
        char netmask_str[INET_ADDRSTRLEN];
        char gateway_str[INET_ADDRSTRLEN];
        char if_to_hop_str[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, router_state->router_table[i].destination, dest_str, sizeof(dest_str));
        inet_ntop(AF_INET, router_state->router_table[i].netmask, netmask_str, sizeof(netmask_str));
        inet_ntop(AF_INET, router_state->router_table[i].gateway, gateway_str, sizeof(gateway_str));
        inet_ntop(AF_INET, router_state->router_table[i].interface, interface_str, sizeof(interface_str));

        log_printf("%-20s %-20s %-20s %-20s %-20u\n",
            dest_str,
            netmask_str,
            gateway_str,
            if_to_hop_str,
            router_state->router_table[i].metric
        );
    }
}

void print_life_table(RouterState *router_state) {
    char interface_str[16];
    snprintf(interface_str, sizeof(interface_str), "%u.%u.%u.%u",
            router_state->interfaces[0].interface_ip[0],
            router_state->interfaces[0].interface_ip[1],
            router_state->interfaces[0].interface_ip[2],
            router_state->interfaces[0].interface_ip[3]
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

int read_riptbl_and_add_to_state(int router_id, RouterState *router_state) {
    char id_str[30];
    snprintf(id_str, sizeof(id_str), "%d", router_id);
    char filename[100] = "router_";
    strncat(filename, id_str, sizeof(filename) - strlen(filename) - 1);
    strncat(filename, ".riptbl", sizeof(filename) - strlen(filename) - 1);

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("riptbl file reading error");
        fclose(file);
        return -1;
    }

    const uint32_t MAX_LINE = 100;
    char line[MAX_LINE];
    uint32_t file_line_count = 0;


    pthread_mutex_lock(&router_state->change_router_table_mutex);
    while (fgets(line, sizeof(line), file)) {
        if (file_line_count >= MAX_NUM_INTERFACES ||
                (line[strlen(line) - 1] != '\n' && !feof(file))
        ) {
            errno = EIO;
            perror("Invalid riptbl file");
            fclose(file);
            return -1;
        }

        char line_ip[20];
        char line_netmask[20];
        sscanf(line, "%s %s", line_ip, line_netmask);
        if (!is_valid_ip(line_ip) || !is_valid_ip(line_netmask)) {
            errno = EIO;
            perror("Invalid riptbl file");
            fclose(file);
            return -1;
        }

        inet_pton(
            AF_INET,
            line_ip,
            router_state->interfaces[router_state->num_interfaces].interface_ip
        );
        inet_pton(
            AF_INET,
            line_netmask,
            router_state->interfaces[router_state->num_interfaces].interface_netmask
        );
        router_state->num_interfaces += 1;

        file_line_count += 1;
    }
    pthread_mutex_unlock(&router_state->change_router_table_mutex);

    fclose(file);
    return 0;
}

RouterState* startup_router(uint32_t router_id) {
    RouterState *router_state = malloc(sizeof(RouterState));
    router_state->router_table = malloc(ROUTER_TABLE_MAX_SIZE * sizeof(RouterTableEntry));

    // interfaces def
    router_state->interfaces = malloc(MAX_NUM_INTERFACES * sizeof(InterfaceTableEntry));
    router_state->num_interfaces = 0;

    // life table def
    router_state->life_table = malloc(ROUTER_TABLE_MAX_SIZE * sizeof(LifeTableEntry));
    router_state->life_entries = 0;

    // router table and additional router state def
    router_state->num_entries = 0;
    router_state->should_restart = 0;
    router_state->should_terminate = 0;
    router_state->router_id = router_id;

    enable_logging = 1;
    uint32_t new_rand_delay = (rand() % 8) + RAND_DELAY_BONUS;
    router_state->rand_delay = new_rand_delay;
    log_printf("router_rand_delay: %u\n", new_rand_delay);

    pthread_mutex_init(&router_state->change_router_table_mutex, NULL);

    int read_riptbl_rc = read_riptbl_and_add_to_state(router_id, router_state);
    if (read_riptbl_rc < 0) {
        free(router_state->router_table);
        free(router_state->life_table);
        free(router_state->interfaces);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        exit(EXIT_FAILURE);
    }

    log_printf("INITIAL_STATE:\n");
    print_router_table(router_state);
    log_printf("\n");

    return router_state;
}

// packet structure:
// 1. 4 bytes -> ip of the interface that is broadcasting
// 2. 4 bytes -> router_id - additional identifier needed for topology grapher
// 3. 4 bytes -> num_entries (uint32_t)
// 4. [router_state->num_entires] times RouterTableEntry for every row
//      in the router table
// 5. RouterTableEntry for the interface of the router as a destination
//      in the router table
//
void* rip_broadcaster(void *arg_router_state) {
    RouterState *router_state = (RouterState*) arg_router_state;

    int sock;
    struct sockaddr_in broadcast_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        free(router_state->router_table);
        free(router_state->life_table);
        free(router_state->interfaces);
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
        free(router_state->interfaces);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        exit(EXIT_FAILURE);
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);

    while (!router_state->should_restart && !router_state->should_terminate) {
        pthread_mutex_lock(&router_state->change_router_table_mutex);

        const uint32_t num_entries_plus_me = router_state->num_entries + 1;

        // check docs for structure of packet being sent
        const uint32_t SIZEOF_PACKET_TO_SEND =
            (num_entries_plus_me * sizeof(RouterTableEntry)) + 12;
        uint8_t *packet_to_send = malloc(SIZEOF_PACKET_TO_SEND);

        memcpy(packet_to_send + 4, &router_state->router_id, 4);
        memcpy(packet_to_send + 8, &num_entries_plus_me, 4);
        memcpy(packet_to_send + 12, router_state->router_table,
               router_state->num_entries * sizeof(RouterTableEntry));

        pthread_mutex_unlock(&router_state->change_router_table_mutex);

        RouterTableEntry myself_to_add;
        inet_pton(AF_INET, "127.0.0.1", &myself_to_add.interface);
        myself_to_add.metric = 0;

        // broadcast address based on every interface
        for (uint32_t i = 0; i < router_state->num_interfaces; i++) {
            uint8_t broadcast_ip[4];
            get_broadcast_ip(
                router_state->interfaces[i].interface_ip,
                router_state->interfaces[i].interface_netmask,
                broadcast_ip
            );
            memcpy(myself_to_add.destination, router_state->interfaces[i].interface_ip, 4);
            memcpy(myself_to_add.netmask, router_state->interfaces[i].interface_netmask, 4);
            memcpy(myself_to_add.gateway, router_state->interfaces[i].interface_ip, 4);

            memcpy(&broadcast_addr.sin_addr.s_addr, broadcast_ip, 4);
            memcpy(packet_to_send, router_state->interfaces[i].interface_ip, 4);
            memcpy(
                &packet_to_send[SIZEOF_PACKET_TO_SEND - sizeof(RouterTableEntry)],
                &myself_to_add,
                sizeof(RouterTableEntry)
            );

            ssize_t sendto_res = sendto(sock, packet_to_send, SIZEOF_PACKET_TO_SEND, 0,
                (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));

            if (sendto_res < 0) {
                perror("sendto failed");
                free(packet_to_send);
                close(sock);
                free(router_state->router_table);
                free(router_state->life_table);
                free(router_state->interfaces);
                pthread_mutex_destroy(&router_state->change_router_table_mutex);
                free(router_state);
                exit(EXIT_FAILURE);
            }
        }
        free(packet_to_send);

        log_printf("Broadcast message sent\n");
        print_router_table(router_state);
        log_printf("\n");
        sleep(router_state->rand_delay);
    }

    close(sock);
    log_printf("rip_broadcaster ended\n");
    return NULL;
}


void* rip_listen(void *arg_rip_listen_state) {
    RipListenState *rip_listen_state = (RipListenState *) arg_rip_listen_state;
    RouterState *router_state = rip_listen_state->router_state;
    uint32_t curr_interface = rip_listen_state->curr_interface;


    int sock;
    struct sockaddr_in listen_addr, sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    uint8_t rec_buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        free(router_state->router_table);
        free(router_state->life_table);
        free(router_state->interfaces);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        free(rip_listen_state);
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
        free(router_state->interfaces);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        free(rip_listen_state);
        exit(EXIT_FAILURE);
    }

    uint8_t broadcast_ip[4];
    get_broadcast_ip(
        router_state->interfaces[curr_interface].interface_ip,
        router_state->interfaces[curr_interface].interface_netmask,
        broadcast_ip
    );

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(BROADCAST_PORT);
    memcpy(
        &listen_addr.sin_addr.s_addr,
        broadcast_ip,
        4
    );

    int bind_res = bind(sock,
            (struct sockaddr*) &listen_addr,
            sizeof(listen_addr)
    );
    if (bind_res < 0) {
        perror("bind failed");
        close(sock);
        free(router_state->router_table);
        free(router_state->life_table);
        free(router_state->interfaces);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        free(rip_listen_state);
        exit(EXIT_FAILURE);
    }

    while (!router_state->should_restart && !router_state->should_terminate) {
        log_printf("Router %u.%u.%u.%u listening for broadcasts on port %d...\n",
                router_state->interfaces[curr_interface].interface_ip[0],
                router_state->interfaces[curr_interface].interface_ip[1],
                router_state->interfaces[curr_interface].interface_ip[2],
                router_state->interfaces[curr_interface].interface_ip[3],
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
            free(router_state->interfaces);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            free(rip_listen_state);
            exit(EXIT_FAILURE);
        }

        // tombstone packet from other packet received
        if (bytes_received == 1 && rec_buffer[0] == 1) {
            continue;
        }

        // tombstone packet form myself was received
        if (router_state->should_restart || router_state->should_terminate) {
            close(sock);
            log_printf("rip_listen ended\n");
            return NULL;
        }

        RouterState *rec_router_state = malloc(sizeof(RouterState));
        rec_router_state->router_table = malloc(ROUTER_TABLE_MAX_SIZE * sizeof(RouterTableEntry));
        rec_router_state->interfaces = malloc(1 * sizeof(InterfaceTableEntry));
        memcpy(rec_router_state->interfaces[0].interface_ip, rec_buffer, 4);

        if (match_ips(
                    rec_router_state->interfaces[0].interface_ip,
                    router_state->interfaces[curr_interface].interface_ip
        )) {
            // ignore router table if it came from me
            free(rec_router_state->router_table);
            free(rec_router_state->interfaces);
            free(rec_router_state);
            continue;
        }
        memcpy(&rec_router_state->router_id, rec_buffer + 4, 4);
        memcpy(&rec_router_state->num_entries, rec_buffer + 8, 4);
        memcpy(rec_router_state->router_table,
                rec_buffer + 12,
                rec_router_state->num_entries * sizeof(RouterTableEntry));

        log_printf("Router %u.%u.%u.%u received on listen\n",
            router_state->interfaces[curr_interface].interface_ip[0],
            router_state->interfaces[curr_interface].interface_ip[1],
            router_state->interfaces[curr_interface].interface_ip[2],
            router_state->interfaces[curr_interface].interface_ip[3]
        );

        pthread_mutex_lock(&router_state->change_router_table_mutex);
        for (uint32_t i = 0; i < rec_router_state->num_entries; i++) {
            if (match_ips(
                        rec_router_state->router_table[i].gateway,
                        router_state->interfaces[curr_interface].interface_ip
            )) {
                // split horizon
                continue;
            }

            int index_of_exact_dest = find_index_of_network_that_exacts(
                        router_state,
                        rec_router_state->router_table[i].destination,
                        rec_router_state->router_table[i].netmask
            );

            int should_do_life_table_update = 1;

            if (index_of_exact_dest != -1) {
                // a network in my router table is exactly the currenty received network
                uint32_t old_metric = router_state->
                    router_table[index_of_exact_dest].metric;
                uint32_t rec_metric = rec_router_state->
                    router_table[i].metric;
                if (match_ips(
                        router_state->router_table[index_of_exact_dest].gateway,
                        rec_router_state->interfaces[0].interface_ip) &&
                    rec_router_state->router_table[i].metric != 0
                ) {
                    // TODO check this
                    // the gateway for this entry is the router i currently receive from,
                    // so i trust the received metric and update even if it is worse
                    router_state->router_table[index_of_exact_dest].metric =
                        cap_metric(rec_metric + 1);
                    memcpy(
                        router_state->router_table[index_of_exact_dest].interface,
                        router_state->interfaces[curr_interface].interface_ip,
                        4
                    );

                } else if (rec_metric + 1 < old_metric) {
                    // the gateway for this entry is being changed, so i have to check if
                    // the new metric is better than the old one before updating
                    memcpy(router_state->router_table[index_of_exact_dest].gateway,
                            rec_router_state->interfaces[0].interface_ip,
                            4
                    );
                    router_state->router_table[index_of_exact_dest].metric =
                        cap_metric(rec_metric + 1);
                    memcpy(
                        router_state->router_table[index_of_exact_dest].interface,
                        router_state->interfaces[curr_interface].interface_ip,
                        4
                    );
                } else {
                    should_do_life_table_update = 0;
                }
            } else {
                int index_of_parent_network = find_index_of_network_that_subsumes(
                        router_state,
                        rec_router_state->router_table[i].destination,
                        rec_router_state->router_table[i].netmask
                );

                if (index_of_parent_network != -1) {
                    // a network in my router table subsumes the currently received network.
                    // add the new network before the parent network in the router table
                    add_to_table_at_pos(
                            router_state, index_of_parent_network,
                            rec_router_state->router_table[i].destination,
                            rec_router_state->router_table[i].netmask,
                            rec_router_state->interfaces[0].interface_ip,
                            router_state->interfaces[curr_interface].interface_ip,
                            cap_metric(rec_router_state->router_table[i].metric + 1)
                    );
                } else {
                    // this is the first time i encounter this network
                    // just add it to the table
                    add_to_table(
                            router_state,
                            rec_router_state->router_table[i].destination,
                            rec_router_state->router_table[i].netmask,
                            rec_router_state->interfaces[0].interface_ip,
                            router_state->interfaces[curr_interface].interface_ip,
                            cap_metric(rec_router_state->router_table[i].metric + 1)
                    );
                }
            }

            if (should_do_life_table_update) {
                if (!life_table_contains_gateway(router_state, rec_router_state->router_table[i].destination)) {
                    add_new_gateway_to_life_table(router_state, rec_router_state->router_table[i].destination);
                } else {
                    reset_gateway_in_life_table(router_state, rec_router_state->router_table[i].destination);
                }
            }
        }
        pthread_mutex_unlock(&router_state->change_router_table_mutex);

        free(rec_router_state->router_table);
        free(rec_router_state->interfaces);
        free(rec_router_state);
    }

    close(sock);
    log_printf("rip_listen ended\n");
    return NULL;
}

void send_tombstone_packets(RouterState *router_state) {
    int sock;
    struct sockaddr_in tombstone_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        free(router_state->router_table);
        free(router_state->life_table);
        free(router_state->interfaces);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
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
        free(router_state->router_table);
        free(router_state->life_table);
        free(router_state->interfaces);
        pthread_mutex_destroy(&router_state->change_router_table_mutex);
        free(router_state);
        exit(EXIT_FAILURE);
    }

    tombstone_addr.sin_family = AF_INET;
    tombstone_addr.sin_port = htons(BROADCAST_PORT);
    for (uint32_t i = 0; i < router_state->num_entries; i++) {
        uint8_t broadcast_ip[4];
        get_broadcast_ip(
            router_state->interfaces[i].interface_ip,
            router_state->interfaces[i].interface_netmask,
            broadcast_ip
        );
        memcpy(&tombstone_addr.sin_addr.s_addr, broadcast_ip, 4);

        const uint32_t SIZEOF_PACKET_TO_SEND = 1;
        const uint8_t data_to_send = 1;
        ssize_t sendto_res =
            sendto(sock, &data_to_send, SIZEOF_PACKET_TO_SEND, 0,
                   (struct sockaddr *)&tombstone_addr, sizeof(tombstone_addr));
        if (sendto_res < 0) {
            perror("sendto failed");
            free(router_state->router_table);
            free(router_state->life_table);
            free(router_state->interfaces);
            pthread_mutex_destroy(&router_state->change_router_table_mutex);
            free(router_state);
            close(sock);
            exit(EXIT_FAILURE);
        }
    }

    close(sock);
    log_printf("Tombstone packets sent\n");
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
        send_tombstone_packets(router_state);
        return CMD_RESTART_ROUTER;
    }
    else if (strcmp(cmd, "exit") == 0) {
        printf("Terminating router...\n");
        router_state->should_terminate = 1;
        send_tombstone_packets(router_state);
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
      // TODO CHANGE LIFE TABLE MUTEX - can be omitted for now since this is
      // the only place where the life_table is being read/written to
      for (uint32_t i = 0; i < router_state->life_entries; i++) {
          if (router_state->life_table[i].life_left == 0) {
              pthread_mutex_lock(&router_state->change_router_table_mutex);
              set_metric_for_all_entries_with_destination(
                      router_state,
                      router_state->life_table[i].gateway,
                      INFINITY_METRIC
              );
              pthread_mutex_unlock(&router_state->change_router_table_mutex);
          } else if (!match_ips(router_state->life_table[i].gateway,
                                router_state->interfaces[0].interface_ip)) {
              router_state->life_table[i].life_left -= 1;
          }
      }
    }

    log_printf("gateway_life_clock ended\n");
    return NULL;
}

/**
 * Threads:
 * 0 -> rip_broadcaster
 * 1 -> listen_for_command
 * 2 -> gateway_life_clock
 * 3,4,5... -> rip_listen
 */
int split_threads(RouterState *router_state) {
    int is_thread_error = 0;
    uint32_t the_router_id = router_state->router_id;
    const uint32_t num_threads = 3 + router_state->num_interfaces;

    pthread_t threads[num_threads];
    RipListenState *rip_listen_states = malloc(router_state->num_interfaces * sizeof(RipListenState));

    for (uint32_t i = 0; i < router_state->num_interfaces; i++) {
        rip_listen_states[i] = (RipListenState) { router_state, i };
        int rc_rip_listen = pthread_create(&threads[3 + i], NULL, rip_listen, (void*) &rip_listen_states[i]);
        if (rc_rip_listen) {
            perror("Error initializing threads.");
            is_thread_error = 1;
            goto cleanup_router_state;
        }
    }

    sleep(1);
    int rc_two = pthread_create(&threads[0], NULL, rip_broadcaster, (void*) router_state);
    if (rc_two) {
        perror("Error initializing threads.");
        is_thread_error = 1;
        goto cleanup_router_state;
    }

    sleep(1);
    int rc_three = pthread_create(&threads[1], NULL, listen_for_command, (void*) router_state);
    if (rc_three) {
        perror("Error initializing threads.");
        is_thread_error = 1;
        goto cleanup_router_state;
    }

    int rc_four = pthread_create(&threads[2], NULL, gateway_life_clock, (void*) router_state);
    if (rc_four) {
        perror("Error initializing threads.");
        is_thread_error = 1;
        goto cleanup_router_state;
    }

    for (uint32_t i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    int was_should_terminate = router_state->should_terminate;

cleanup_router_state:
    free(rip_listen_states);
    free(router_state->router_table);
    free(router_state->life_table);
    free(router_state->interfaces);
    pthread_mutex_destroy(&router_state->change_router_table_mutex);
    free(router_state);

    if (is_thread_error) {
        // there was a thread initialization error
        return -1;
    } else if (was_should_terminate) {
        // the user requested for the router to terminate
        return 0;
    } else {
        // recursive call - the router should restart
        RouterState *reloaded_router = startup_router(the_router_id);
        return split_threads(reloaded_router);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        errno = EINVAL;
        perror("Invalid arguments");
        exit(EXIT_FAILURE);
    }

    setbuf(stdout, NULL);
    srand(time(NULL));

    if (strcmp(argv[1], "router") == 0) {
        uint32_t curr_num_router = atoi(argv[2]);
        RouterState *router_one = startup_router(curr_num_router);
        int split_rc = split_threads(router_one);
        if (split_rc < 0) {
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(argv[1], "host") == 0) {
        uint32_t curr_num_host = atoi(argv[2]);
        HostState *host_one = startup_host(curr_num_host);
        int host_split_rc = host_split_threads(host_one);
        if (host_split_rc < 0) {
            exit(EXIT_FAILURE);
        }
    }
    else {
        errno = EINVAL;
        perror("Invalid arguments");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
