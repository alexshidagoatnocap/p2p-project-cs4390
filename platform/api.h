#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  void *storage;
  uint32_t length;
} SocketAddress;

void initSocketAPI();

void cleanupSocketAPI();

SocketAddress *createIPV4Addr(const char *ip, uint16_t port);

int32_t createIPV4SockStream();

void removeIPV4Addr(SocketAddress *addr);

int32_t connectToSocket(int32_t sockfd, const SocketAddress *address);

uint32_t hostToNetLong(uint32_t hostlong);

uint32_t netToHostLong(uint32_t netlong);

size_t sendSocket(int32_t sockfd, const void *buffer, uint32_t len,
                  int32_t flags);

size_t recvSocket(int32_t sockfd, const void *buffer, uint32_t len,
                  int32_t flags);

int32_t bindSocket(int32_t sockfd, const SocketAddress *address);

int32_t listenToSocket(int32_t sockfd, int32_t n);

int32_t acceptSocket(int32_t sockfd, SocketAddress *address);

void closeSocket(int32_t sockfd);

void shutdownSocketRDWR(int32_t sockfd);
