#pragma once

#include "protocol.h"
#include <stddef.h>
#include <sys/types.h>

#define TRACKER_MAX_PATH 512

typedef struct {
    int client_fd;
    char torrents_dir[TRACKER_MAX_PATH];
} TrackerWorkerArgs;

int send_all(int fd, const void *buf, size_t len);
ssize_t recv_line(int fd, char *buf, size_t maxlen);
