#include "tracker.h"
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

  SocketAddress *clientAddress = createIPV4Addr("", 0);
  int32_t clientSocketFD = acceptSocket(serverSocketFD, clientAddress);
  if (clientSocketFD < 0) {
    printf("Tracker failed to connect to peer.\n");
    exit(EXIT_FAILURE);
  }

  char buffer[2048];
  while (true) {
    size_t recvMsgSize = recvSocket(clientSocketFD, buffer, sizeof(buffer), 0);
    if (recvMsgSize > 0) {
      buffer[recvMsgSize] = 0;
      printf("Response from Peer:\n%s", buffer);
    }

    if (strcmp(buffer, "exit\n") == 0) {
      break;
    }

    if (recvMsgSize < 0) {
      break;
    }
  }

  closeSocket(clientSocketFD);
  shutdownSocketRDWR(serverSocketFD);
  removeIPV4Addr(clientAddress);
  removeIPV4Addr(serverAddress);

  cleanupSocketAPI();
  return 0;
}

AcceptedSocket *acceptIncomingConnection(uint32_t serverSocketFD) {
  SocketAddress *clientAddress = createIPV4Addr("", 0);
  int32_t clientSocketFD = acceptSocket(serverSocketFD, clientAddress);

  AcceptedSocket *acceptedSocket = malloc(sizeof(AcceptedSocket));
  acceptedSocket->sockAddr = *clientAddress;
  acceptedSocket->acceptedFD = clientSocketFD;
  if (acceptedSocket->acceptedFD < 0) {
    acceptedSocket->status = SOCKET_ERROR;
  }

  return acceptedSocket;
}
