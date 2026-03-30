#include "peer.h"
#include "api.h"
#include "protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CommandStatus recvTrackerFile(int32_t socketFD) {
  char tfName[TRK_FNAME_SIZE];
  char tfPath[TRK_FNAME_SIZE];
  recvSocket(socketFD, tfName, TRK_FNAME_SIZE, 0);
  snprintf(tfPath, sizeof(tfPath), "peer/trk/%s.trk", tfName);

  uint32_t fileSize;
  uint32_t fileSizeNet;
  recvSocket(socketFD, &fileSizeNet, sizeof(fileSizeNet), 0);
  fileSize = netToHostLong(fileSizeNet);

  FILE *tFile = fopen(tfPath, "wb");
  char fileBuffer[CHUNK_SIZE];
  uint32_t totalRecv = 0;

  while (totalRecv < fileSize) {
    size_t bytesRecv = recvSocket(socketFD, fileBuffer, sizeof(fileBuffer), 0);
    if (bytesRecv <= 0)
      break;
    fwrite(fileBuffer, 1, bytesRecv, tFile);
    totalRecv += bytesRecv;
  }

  fclose(tFile);
  return STATUS_OK;
}

int main() {
  initSocketAPI();
  printf("Hello from Peer!\n");

  int32_t socketFD = createIPV4SockStream();
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
  char *savePtr = NULL;
  size_t lineSize = 0;
  char msgFromTrk[BUFFER_SIZE];
  while (true) {
    auto charCount = getline(&line, &lineSize, stdin);
    if (charCount > 0) {
      sendSocket(socketFD, line, charCount, 0);
    }
    strtok_r(line, " \n", &savePtr);

    if (strcmp(line, "exit") == 0) {
      break;
    } else if (strcmp(line, "get") == 0) {
      // First receive the response to check if GET succeeded
      recvSocket(socketFD, msgFromTrk, BUFFER_SIZE, 0);

      // Only try to receive file if response indicates success
      if (strncmp(msgFromTrk, "REP GET BEGIN", 13) == 0) {
        // GET was successful, receive the file
        recvTrackerFile(socketFD);
      }

      // Print the response message
      printf("%s\n", msgFromTrk);
      continue;
    } else if (strcmp(line, "createtracker") == 0) {
      recvSocket(socketFD, msgFromTrk, BUFFER_SIZE, 0);
      printf("%s\n", msgFromTrk);
      continue;
    } else if (strcmp(line, "updatetracker") == 0) {
      recvSocket(socketFD, msgFromTrk, BUFFER_SIZE, 0);
      printf("%s\n", msgFromTrk);
      continue;
    } else if (strcmp(line, "list") == 0) {
      recvSocket(socketFD, msgFromTrk, BUFFER_SIZE, 0);
      printf("%s\n", msgFromTrk);
      continue;
    } else {
      continue;
    }

    removeIPV4Addr(address);

    closeSocket(socketFD);

    cleanupSocketAPI();
    return 0;
  }
}
