#include "tracker.h"
#include "api.h"
#include "protocol.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#elif __linux__
#include <sys/socket.h>
#endif

TrackerInfo trackerArray_g[256];

atomic_int activePeers = 0;
mtx_t peerMutex;
cnd_t peerCnd;

static void receiveClientMsgs(int32_t clientSocketFD) {
  char buffer[2048];
  while (true) {
    size_t recvMsgSize = recvSocket(clientSocketFD, buffer, sizeof(buffer), 0);
    if (recvMsgSize > 0) {
      buffer[strcspn(buffer, "\n")] = 0;
      printf("Response from Peer:\n%s", buffer);
      if (parseCommand(buffer).Type == CMD_EXIT) {
        break;
      };
    }

    if (recvMsgSize < 0) {
      break;
    }
  }
}

static int32_t peerThread(void *arg) {
  int32_t peerSocketFD = *(int32_t *)arg;
  free(arg);
  receiveClientMsgs(peerSocketFD);
  closeSocket(peerSocketFD);

  atomic_fetch_sub(&activePeers, 1);
  return 0;
}

CommandStatus createTracker(TrackerInfo tr) {
  FILE *tFile;
  char tfName[512];
  snprintf(tfName, sizeof(tfName), "tracker/trk/%s.trk", tr.filename);

  tFile = fopen(tfName, "wx");
  if (tFile == NULL) {
    printf("Tracker file already exists\n");
    return STATUS_FILE_ERROR;
  }

  fprintf(tFile, "Filename: %s\n", tr.filename);
  fprintf(tFile, "Filesize: %zu\n", tr.filesize);
  fprintf(tFile, "Description: %s\n", tr.description);
  fprintf(tFile, "MD5: %s\n", tr.md5Hash);

  for (size_t i = 0; i < tr.numPeers; ++i) {
    fprintf(tFile, "%s:%u:%zu:%zu:%ld\n", tr.peers[i].ip, tr.peers[i].port,
            tr.peers[i].startByte, tr.peers[i].endByte, tr.peers[i].timestamp);
  }

  return STATUS_OK;
}

static void testCreateTracker() {
  TrackerInfo testTr = {.filename = "for-whom-the-bell-tolls.mp3",
                        .filesize = 4000000,
                        .description = "plz lars dont sue me",
                        .md5Hash = "ABADBABE",
                        .numPeers = 1,
                        .peers[0] = {.ip = "127.0.0.1",
                                     .port = 9001,
                                     .startByte = 0,
                                     .endByte = 20000,
                                     .timestamp = time(NULL)}};
  createTracker(testTr);
}

int main() {
  testCreateTracker();
  // initSocketAPI();
  // printf("Hello from Tracker!\n");
  //
  // int32_t serverSocketFD = createSocketFileDescriptor(AF_INET, SOCK_STREAM,
  // 0); if (serverSocketFD == -1) {
  //   printf("Tracker Socket Failed!\n");
  //   exit(EXIT_FAILURE);
  // }
  //
  // SocketAddress *serverAddress = createIPV4Addr("", 2000);
  //
  // if (bindSocket(serverSocketFD, serverAddress) < 0) {
  //   printf("Tracker Socket Bound Failed!\n");
  //   exit(EXIT_FAILURE);
  // }
  //
  // printf("Tracker Socket Successful!\n");
  //
  // if (listenToSocket(serverSocketFD, 10) < 0) {
  //   printf("Tracker Cannot Listen!\n");
  //   exit(EXIT_FAILURE);
  // }
  //
  // while (true) {
  //   SocketAddress *clientAddress = createIPV4Addr("", 0);
  //   int32_t clientSocketFD = acceptSocket(serverSocketFD, clientAddress);
  //   removeIPV4Addr(clientAddress);
  //
  //   if (clientSocketFD < 0) {
  //     printf("Tracker failed to connect to peer.\n");
  //     continue;
  //   }
  //
  //   int32_t *clientSocketFDptr = malloc(sizeof(int32_t));
  //   *clientSocketFDptr = clientSocketFD;
  //
  //   thrd_t peerID;
  //   thrd_create(&peerID, peerThread, clientSocketFDptr);
  //   atomic_fetch_add(&activePeers, 1);
  //   thrd_join(peerID, NULL);
  // }
  //
  // shutdownSocketRDWR(serverSocketFD);
  // removeIPV4Addr(serverAddress);
  //
  // cleanupSocketAPI();
  // return 0;
}
