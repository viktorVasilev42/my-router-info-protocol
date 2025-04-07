#include "first.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <termios.h>

const uint32_t BROADCAST_PORT = 12345;
const uint32_t LIVENESS_PORT = 12346;
const uint32_t BUFFER_SIZE = 1024;
const uint32_t ROUTER_TABLE_MAX_SIZE = 100;
const uint32_t INFINITY_METRIC = 16;
const uint32_t MAX_GATEWAY_LIFE = 5;
const uint32_t TIME_FOR_LIFE_DROP = 3;
const uint32_t RAND_DELAY_BONUS = 3;

int enable_logging = 1;

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

void get_broadcast_ip(uint8_t *host_ip, uint8_t *netmask, uint8_t *broadcast_ip) {
    for (int i = 0; i < 4; i++) {
        broadcast_ip[i] = host_ip[i] | (~netmask[i]);
    }
}

void log_printf(const char *format, ...) {
    if (!enable_logging) return;

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void enable_raw_term() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void disable_raw_term() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

uint32_t cap_metric(uint32_t metric_to_cap) {
    if (metric_to_cap > INFINITY_METRIC) {
        return INFINITY_METRIC;
    }

    return metric_to_cap;
}
