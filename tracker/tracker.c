#include "tracker.h"
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
  printf("Hello from Tracker!\n");

  int32_t serverSocketFD = createSocketFileDescriptor(AF_INET, SOCK_STREAM, 0);
  if (serverSocketFD == -1) {
    printf("Tracker Socket Failed!\n");
    exit(EXIT_FAILURE);
  }

  SocketAddress *serverAddress = createIPV4Addr("", 2000);

  if (bindSocket(serverSocketFD, serverAddress) < 0) {
    printf("Tracker Socket Bound Failed!\n");
    exit(EXIT_FAILURE);
  }

  printf("Tracker Socket Successful!\n");

  if (listenToSocket(serverSocketFD, 10) < 0) {
    printf("Tracker Cannot Listen!\n");
    exit(EXIT_FAILURE);
  }

  SocketAddress clientAddress;
  int32_t clientSocketFD = acceptSocket(serverSocketFD, &clientAddress);

  char buffer[2048];
  recvSocket(clientSocketFD, buffer, sizeof(buffer), 0);
  printf("Response from Peer:\n%s", buffer);

  closeSocket(serverSocketFD);

  cleanupSocketAPI();
  return 0;
}
