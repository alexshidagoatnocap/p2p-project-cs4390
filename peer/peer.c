#include "peer.h"
#include "linux_api.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

int main() {
  printf("Hello from Peer!\n");

  int socketFD = socket(AF_INET, SOCK_STREAM, 0);
  if (socketFD == -1) {
    printf("Peer Socket Failed!\n");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in *address = createIPV4Addr("142.250.188.46", 80);

  int connectStatus =
      connect(socketFD, (struct sockaddr *)address, sizeof(*address));
  if (connectStatus == -1) {
    printf("Peer Connection Failed!\n");
    exit(EXIT_FAILURE);
  }

  printf("Peer Connection Successful! \n");

  char httpMsg[] = "GET \\ HTTP/1.1\r\nHost:google.com\r\n\r\n";
  send(socketFD, httpMsg, sizeof(httpMsg), 0);

  char buffer[2048];
  recv(socketFD, buffer, sizeof(buffer) - 1, 0);

  printf("%s\n", buffer);
  removeIPV4Addr(address);

  return 0;
}
