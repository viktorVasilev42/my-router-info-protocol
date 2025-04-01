#ifndef HOST_H
#define HOST_H

#include <stdint.h>

typedef struct {
    uint8_t interface_ip[4];
    uint8_t interface_netmask[4];
    uint32_t rand_delay;
} HostState;

extern const uint32_t HOST_RAND_DELAY_BONUS;

HostState* startup_host(uint32_t host_id);

int host_split_threads(HostState *host_state);

#endif
