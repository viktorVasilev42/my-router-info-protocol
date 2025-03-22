#ifndef FIRST_H
#define FIRST_H

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

// life table
typedef struct {
    uint8_t gateway[4];
    uint32_t life_left;
} LifeTableEntry;

// routing table
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
    LifeTableEntry *life_table;
    uint32_t num_entries;
    uint32_t life_entries;
    uint32_t rand_delay;
    pthread_mutex_t change_router_table_mutex;
    int should_restart;
    int should_terminate;
} RouterState;

// command return status
typedef enum {
    CMD_DONE,
    CMD_DONT_RESTORE_SHOULD_LOG,
    CMD_RESTART_ROUTER,
    CMD_TERMINATE
} HandleCmdReturnCode;

extern const uint32_t BROADCAST_PORT;
extern const uint32_t LIVENESS_PORT;
extern const uint32_t BUFFER_SIZE;
extern const uint32_t ROUTER_TABLE_MAX_SIZE;
extern const uint32_t INFINITY_METRIC;
extern const uint32_t MAX_GATEWAY_LIFE;
extern const uint32_t TIME_FOR_LIFE_DROP;

extern int enable_logging;


int match_ips(uint8_t *first_ip, uint8_t *second_ip);

int is_valid_ip(const char *ip);


int is_network_subsumed(uint8_t *ip1, uint8_t *mask1, uint8_t *ip2, uint8_t *mask2);

void get_broadcast_ip(uint8_t *host_ip, uint8_t *netmask, uint8_t *broadcast_ip);

void log_printf(const char *format, ...);

void enable_raw_term();

void disable_raw_term();

uint32_t cap_metric(uint32_t metric_to_cap);

#endif
