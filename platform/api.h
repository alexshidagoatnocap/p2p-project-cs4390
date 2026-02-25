#pragma once

#include <stdint.h>
#include <sys/socket.h>

void initSocketAPI();

void cleanupSocketAPI();

int32_t createSocketFileDescriptor(uint32_t domain, uint32_t type,
                                   uint32_t protocol);

struct sockaddr_in *createIPV4Addr(const char *ip, uint16_t port);

void removeIPV4Addr(struct sockaddr_in *addr);

typedef struct {
  void *storage;
  uint32_t length;
} SocketAddress;

int32_t connectToSocket(int32_t sockfd, const SocketAddress *address);
