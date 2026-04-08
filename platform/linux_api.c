#include "api.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static int32_t createSocketFileDescriptor(int32_t domain, int32_t type,
                                          int32_t protocol) {
  int32_t sockfd = socket(domain, type, protocol);
  if (sockfd == -1) {
    printf("Socket Creation Failed!\n");
  }
  return sockfd;
}

int32_t createIPV4SockStream() {
  return createSocketFileDescriptor(AF_INET, SOCK_STREAM, 0);
}

SocketAddress *createIPV4Addr(const char *ip, uint16_t port) {
  struct sockaddr_in *addr_platform = malloc(sizeof(struct sockaddr_in));

  addr_platform->sin_port = htons(port);
  addr_platform->sin_family = AF_INET;

  if (strlen(ip) == 0) {
    addr_platform->sin_addr.s_addr = INADDR_ANY;
  }

  inet_pton(addr_platform->sin_family, ip, &addr_platform->sin_addr.s_addr);

  SocketAddress *address = malloc(sizeof(SocketAddress));

  address->storage = addr_platform;
  address->length = sizeof(*addr_platform);

  return address;
}

void removeIPV4Addr(SocketAddress *addr) {
  free(addr->storage);
  addr->storage = NULL;
  free(addr);
  addr = NULL;
}

int32_t connectToSocket(int32_t sockfd, const SocketAddress *address) {
  const struct sockaddr *castAddr = (const struct sockaddr *)address->storage;
  return connect(sockfd, castAddr, address->length);
}

uint32_t hostToNetLong(uint32_t hostlong) { return htonl(hostlong); }

uint32_t netToHostLong(uint32_t netlong) { return ntohl(netlong); }

size_t sendSocket(int32_t sockfd, const void *buffer, uint32_t len,
                  int32_t flags) {
  size_t status = send(sockfd, (void *)buffer, len, flags);
  return status;
}

size_t recvSocket(int32_t sockfd, const void *buffer, uint32_t len,
                  int32_t flags) {
  size_t status = recv(sockfd, (void *)buffer, len, 0);
  return status;
}

int32_t bindSocket(int32_t sockfd, const SocketAddress *address) {
  const struct sockaddr *castAddr = (const struct sockaddr *)address->storage;
  return bind(sockfd, castAddr, address->length);
}

int32_t listenToSocket(int32_t sockfd, int32_t n) { return listen(sockfd, n); }

int32_t acceptSocket(int32_t sockfd, SocketAddress *address) {
  struct sockaddr *castAddr = (struct sockaddr *)address->storage;
  return accept(sockfd, castAddr, &address->length);
}

void closeSocket(int32_t sockfd) { close(sockfd); }

void shutdownSocketRDWR(int32_t sockfd) { shutdown(sockfd, SHUT_RDWR); }

// Stub functions for initializing a socket API, no need to init on linux
void initSocketAPI() {}
void cleanupSocketAPI() {}
