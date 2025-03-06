#include <stdint.h>
#include "first.h"
#include <arpa/inet.h>

const uint32_t BROADCAST_PORT = 12345;
const uint32_t BUFFER_SIZE = 1024;
const uint32_t ROUTER_TABLE_MAX_SIZE = 100;

int match_ips(uint8_t *first_ip, uint8_t *second_ip) {
    for (int i = 0; i < 4; i++) {
        if (first_ip[i] != second_ip[i]) {
            return 0;
        }
    }

    return 1;
}

int is_valid_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) == 1;
}

void get_broadcast_ip(uint8_t *host_ip, uint8_t *netmask, uint8_t *broadcast_ip) {
    for (int i = 0; i < 4; i++) {
        broadcast_ip[i] = host_ip[i] | (~netmask[i]);
    }
}
