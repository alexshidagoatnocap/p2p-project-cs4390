#pragma once

#include <stdint.h>

struct sockaddr_in *createIPV4Addr(const char *ip, uint16_t port);

void removeIPV4Addr(struct sockaddr_in *addr);
