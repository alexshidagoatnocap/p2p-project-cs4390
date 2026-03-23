#include "peer.h"
#include "api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#elif __linux__
#include <sys/socket.h>
#endif

int main(int argc, char *argv[]) {
  initSocketAPI();
  printf("Hello from Peer!\n");

  int32_t socketFD = createSocketFileDescriptor(AF_INET, SOCK_STREAM, 0);
  if (socketFD == -1) {
    printf("Peer Socket Failed!\n");
    exit(EXIT_FAILURE);
  }

  SocketAddress *address = createIPV4Addr("127.0.0.1", 2000);

  int32_t connectStatus = connectToSocket(socketFD, address);
  if (connectStatus < 0) {
    printf("Peer Connection Failed!\n");
    exit(EXIT_FAILURE);
  }

  printf("Peer Connection Successful! \n");

  char *line = NULL;
  size_t lineSize;
  while (true) {
    size_t charCount = getline(&line, &lineSize, stdin);
    if (charCount > 0) {
      sendSocket(socketFD, line, charCount, 0);
    }

    if (strcmp(line, "exit\n") == 0) {
      break;
    }
  }

  removeIPV4Addr(address);

  closeSocket(socketFD);

  cleanupSocketAPI();
  return 0;
}
