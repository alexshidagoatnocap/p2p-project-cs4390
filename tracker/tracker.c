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

TrackerInfo trackerArray_g[MAX_TRACKER_FILES];
atomic_int numTrackerFiles_g = 0;

atomic_int activePeers_g = 0;
mtx_t trkMutex;
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

  atomic_fetch_sub(&activePeers_g, 1);
  return 0;
}

CommandStatus createTrackerFile(size_t trackerId) {
  TrackerInfo *tr;
  if (mtx_lock(&trkMutex) != thrd_success) {
    return STATUS_FAIL;
  }
  tr = &trackerArray_g[trackerId];
  // atomic_fetch_add(&numTrackerFiles_g, 1);

  FILE *tFile;
  char tfName[512];
  snprintf(tfName, sizeof(tfName), "tracker/trk/%s.trk", tr->filename);

  tFile = fopen(tfName, "wx");
  if (tFile == NULL) {
    printf("Tracker file already exists for %s\n", tr->filename);
    mtx_unlock(&trkMutex);
    return STATUS_FILE_ERROR;
  }

  fprintf(tFile, "Filename: %s\n", tr->filename);
  fprintf(tFile, "Filesize: %zu\n", tr->filesize);
  fprintf(tFile, "Description: %s\n", tr->description);
  fprintf(tFile, "MD5: %s\n", tr->md5Hash);

  for (size_t i = 0; i < tr->numPeers; ++i) {
    fprintf(tFile, "%s:%u:%zu:%zu:%ld\n", tr->Peers[i].ip, tr->Peers[i].port,
            tr->Peers[i].startByte, tr->Peers[i].endByte,
            tr->Peers[i].timestamp);
  }
  mtx_unlock(&trkMutex);

  fclose(tFile);

  return STATUS_OK;
}

CommandStatus updateTrackerFile(size_t trackerId) {
  TrackerInfo *tr;
  if (mtx_lock(&trkMutex) != thrd_success) {
    return STATUS_FAIL;
  }
  tr = &trackerArray_g[trackerId];
  // atomic_fetch_add(&numTrackerFiles_g, 1);

  FILE *tFile;
  char tfName[512];
  snprintf(tfName, sizeof(tfName), "tracker/trk/%s.trk", tr->filename);
  tFile = fopen(tfName, "r+");
  if (tFile == NULL) {
    // TODO: Close TCP connection and terminate handler thread
    printf("Tracker file does not exist for %s\n", tr->filename);
    mtx_unlock(&trkMutex);
    return STATUS_FILE_ERROR;
  }

  fprintf(tFile, "Filename: %s\n", tr->filename);
  fprintf(tFile, "Filesize: %zu\n", tr->filesize);
  fprintf(tFile, "Description: %s\n", tr->description);
  fprintf(tFile, "MD5: %s\n", tr->md5Hash);

  for (size_t i = 0; i < tr->numPeers; ++i) {
    fprintf(tFile, "%s:%u:%zu:%zu:%ld\n", tr->Peers[i].ip, tr->Peers[i].port,
            tr->Peers[i].startByte, tr->Peers[i].endByte,
            tr->Peers[i].timestamp);
  }

  fclose(tFile);
  mtx_unlock(&trkMutex);

  return STATUS_FAIL;
}

static CommandStatus sendTrackerFile(char *tfName, FILE *tFile,
                                     int32_t peerSockFD) {
  if (tFile == NULL) {
    perror("Cannot send file, FP does not exist.");
    return STATUS_FILE_ERROR;
  }

  sendSocket(peerSockFD, tfName, TRK_FNAME_SIZE, 0);

  fseek(tFile, 0, SEEK_END);
  uint32_t tfSize = ftell(tFile);
  rewind(tFile);
  uint32_t tfNetSize = hostToNetLong(tfSize);
  sendSocket(peerSockFD, &tfNetSize, sizeof(tfNetSize), 0);

  char sendBuffer[CHUNK_SIZE];
  size_t bytesRead = 0;
  while ((bytesRead = fread(sendBuffer, 1, sizeof(sendBuffer), tFile)) > 0) {
    if (sendSocket(peerSockFD, sendBuffer, bytesRead, 0) == -1) {
      perror("Error sending tracker file!");
      return STATUS_FAIL;
    }
  }
  return STATUS_OK;
}

CommandStatus getAndSendTrackerInfo(TrackerInfo *tr, int32_t peerSockFD) {
  if (mtx_lock(&trkMutex) != thrd_success) {
    return STATUS_FAIL;
  }

  FILE *tFile;
  char tfName[TRK_FNAME_SIZE];
  snprintf(tfName, sizeof(tfName), "tracker/trk/%s.trk", tr->filename);
  tFile = fopen(tfName, "rb");
  if (tFile == NULL) {
    printf("Tracker file does not exist for %s\n", tr->filename);
    mtx_unlock(&trkMutex);
    return STATUS_FILE_ERROR;
  }

  sendTrackerFile(tr->filename, tFile, peerSockFD);

  fclose(tFile);
  mtx_unlock(&trkMutex);
  return STATUS_OK;
}

static void testCreateAndUpdateTracker() {
  TrackerInfo testTr = {.filename = "for-whom-the-bell-tolls.mp3",
                        .filesize = 4000000,
                        .description = "plz lars dont sue me",
                        .md5Hash = "ABADBABE",
                        .numPeers = 1,
                        .trackerId = 1,
                        .Peers[0] = {.ip = "127.0.0.1",
                                     .port = 9001,
                                     .startByte = 0,
                                     .endByte = 20000,
                                     .timestamp = time(NULL)}};
  trackerArray_g[testTr.trackerId] = testTr;
  createTrackerFile(testTr.trackerId);

  TrackerInfo testTr2 = {.filename = "for-whom-the-bell-tolls.mp3",
                         .filesize = 4000000,
                         .description = "plz lars dont sue me",
                         .md5Hash = "ABADBABE",
                         .numPeers = 2,
                         .trackerId = 1,
                         .Peers[0] = {.ip = "127.0.0.2",
                                      .port = 9001,
                                      .startByte = 0,
                                      .endByte = 20000,
                                      .timestamp = time(NULL)},
                         .Peers[1] = {.ip = "192.168.1.2",
                                      .port = 67,
                                      .startByte = 20001,
                                      .endByte = 40000,
                                      .timestamp = time(NULL)}};
  trackerArray_g[testTr2.trackerId] = testTr2;
  updateTrackerFile(testTr2.trackerId);
}

int main() {
  if (mtx_init(&trkMutex, mtx_plain) != thrd_success) {
    printf("Tracker Mutex init failed");
    return 1;
  }
  testCreateAndUpdateTracker();

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

  while (true) {
    SocketAddress *clientAddress = createIPV4Addr("", 0);
    int32_t clientSocketFD = acceptSocket(serverSocketFD, clientAddress);
    removeIPV4Addr(clientAddress);

    if (clientSocketFD < 0) {
      printf("Tracker failed to connect to peer.\n");
      continue;
    }

    getAndSendTrackerInfo(&trackerArray_g[1], clientSocketFD);
    int32_t *clientSocketFDptr = malloc(sizeof(int32_t));
    *clientSocketFDptr = clientSocketFD;

    thrd_t peerID;
    thrd_create(&peerID, peerThread, clientSocketFDptr);
    atomic_fetch_add(&activePeers_g, 1);
    thrd_join(peerID, NULL);
  }

  shutdownSocketRDWR(serverSocketFD);
  removeIPV4Addr(serverAddress);

  cleanupSocketAPI();
  return 0;
}
