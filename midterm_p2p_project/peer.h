#pragma once

#include "protocol.h"
#include <stdint.h>
#include <sys/types.h>

#define PEER_MAX_PATH 512

typedef struct {
    uint16_t tracker_port;
    char tracker_ip[64];
    int update_interval_seconds;
} ClientConfig;

typedef struct {
    uint16_t listen_port;
    char shared_folder[PEER_MAX_PATH];
} ServerConfig;

int send_all(int fd, const void *buf, size_t len);
ssize_t recv_line(int fd, char *buf, size_t maxlen);
