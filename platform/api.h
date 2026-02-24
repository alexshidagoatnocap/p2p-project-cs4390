#pragma once

#include <stdint.h>

void initSocketAPI();

void cleanupSocketAPI();

struct sockaddr_in *createIPV4Addr(const char *ip, uint16_t port);

void removeIPV4Addr(struct sockaddr_in *addr);
