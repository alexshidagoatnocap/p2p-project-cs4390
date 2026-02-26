#include "peer.h"
#include "api.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#elif __linux__
#include <sys/socket.h>
#endif

int main() {
  initSocketAPI();
  printf("Hello from Peer!\n");

  int32_t socketFD = createSocketFileDescriptor(AF_INET, SOCK_STREAM, 0);
  if (socketFD == -1) {
    printf("Peer Socket Failed!\n");
    exit(EXIT_FAILURE);
  }

  void *address = createIPV4Addr("142.250.188.46", 80);

  int32_t connectStatus = connectToSocket(socketFD, address);
  if (connectStatus == -1) {
    printf("Peer Connection Failed!\n");
    exit(EXIT_FAILURE);
  }

  printf("Peer Connection Successful! \n");

  char httpMsg[] = "GET \\ HTTP/1.1\r\nHost:google.com\r\n\r\n";
  sendSocket(socketFD, httpMsg, sizeof(httpMsg), 0);

  char buffer[2048];
  recvSocket(socketFD, buffer, sizeof(buffer) - 1, 0);

  printf("%s\n", buffer);
  removeIPV4Addr(address);

  closeSocket(socketFD);

  cleanupSocketAPI();
  return 0;
}
