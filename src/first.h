#ifndef FIRST_H
#define FIRST_H

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

typedef struct {
    uint8_t destination[4];
    uint8_t netmask[4];
    uint8_t gateway[4];
    uint32_t metric;
} RouterTableEntry;

typedef struct {
    uint32_t router_id;
    uint8_t interface_ip[4];
    uint8_t interface_netmask[4];
    RouterTableEntry *router_table;
    uint32_t num_entries;
    uint32_t rand_delay;
    pthread_mutex_t change_router_table_mutex;
    int should_restart;
} RouterState;

typedef enum {
    CMD_DONT_RESTORE_SHOULD_LOG,
    CMD_RESTART_ROUTER,
    CMD_DONE
} HandleCmdReturnCode;

extern const uint32_t BROADCAST_PORT;
extern const uint32_t BUFFER_SIZE;
extern const uint32_t ROUTER_TABLE_MAX_SIZE;

extern int enable_logging;

int match_ips(uint8_t *first_ip, uint8_t *second_ip);

int is_valid_ip(const char *ip);


int is_network_subsumed(uint8_t *ip1, uint8_t *mask1, uint8_t *ip2, uint8_t *mask2);

void get_broadcast_ip(uint8_t *host_ip, uint8_t *netmask, uint8_t *broadcast_ip);

void log_printf(const char *format, ...);

void enable_raw_term();

void disable_raw_term();

#endif
