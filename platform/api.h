#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  void *storage;
  uint32_t length;
} SocketAddress;

void initSocketAPI();

void cleanupSocketAPI();

int32_t createSocketFileDescriptor(uint32_t domain, uint32_t type,
                                   uint32_t protocol);

SocketAddress *createIPV4Addr(const char *ip, uint16_t port);

void removeIPV4Addr(SocketAddress *addr);

int32_t connectToSocket(int32_t sockfd, const SocketAddress *address);

size_t sendToSocket(int32_t sockfd, const char *buffer, uint32_t len,
                    int32_t flags);

size_t recvFromSocket(int32_t sockfd, const char *buffer, uint32_t len,
                      int32_t flags);

void closeSocket(int32_t sockfd);
