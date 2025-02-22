#include <stdint.h>
#include "first.h"

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
