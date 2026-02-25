#include "api.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

int32_t createSocketFileDescriptor(uint32_t domain, uint32_t type,
                                   uint32_t protocol) {
  int32_t sockfd = socket(domain, type, protocol);
  if (sockfd == -1) {
    printf("Socket Creation Failed!\n");
  }
  return sockfd;
}

struct sockaddr_in *createIPV4Addr(const char *ip, uint16_t port) {
  struct sockaddr_in *address = malloc(sizeof(struct sockaddr_in));

  address->sin_port = htons(port);
  address->sin_family = AF_INET;

  inet_pton(address->sin_family, ip, &address->sin_addr.s_addr);

  return address;
}

void removeIPV4Addr(struct sockaddr_in *addr) { free(addr); }

int32_t connectToSocket(int32_t sockfd, const SocketAddress *address) {
  struct sockaddr *castAddr = (struct sockaddr *)&address->storage;
  return connect(sockfd, castAddr, address->length);
}

// Stub functions for initializing a socket API, no need to init on linux
void initSocketAPI() {}
void cleanupSocketAPI() {}
