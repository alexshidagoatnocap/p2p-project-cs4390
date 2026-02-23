#include "linux_api.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

struct sockaddr_in *createIPV4Addr(const char *ip, uint16_t port) {
  struct sockaddr_in *address = malloc(sizeof(struct sockaddr_in));

  address->sin_port = htons(port);
  address->sin_family = AF_INET;

  inet_pton(address->sin_family, ip, &address->sin_addr.s_addr);

  return address;
}

void removeIPV4Addr(struct sockaddr_in *addr) { free(addr); }
