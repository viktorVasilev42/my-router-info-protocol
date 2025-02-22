#ifndef FIRST_H
#define FIRST_H

#include <stdint.h>

extern const uint32_t BROADCAST_PORT;
extern const uint32_t BUFFER_SIZE;
extern const uint32_t ROUTER_TABLE_MAX_SIZE;

int match_ips(uint8_t *first_ip, uint8_t *second_ip);

#endif
