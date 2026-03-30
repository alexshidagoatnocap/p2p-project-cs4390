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

// ============ STATE GLOBALS ============
TrackerInfo trackerArray_g[MAX_TRACKER_FILES];
atomic_int numTrackerFiles_g = 0;
mtx_t trkMutex;

atomic_int activePeers_g = 0;
mtx_t peerMutex;
cnd_t peerCnd;

// ============ TRACKER LOOKUP UTILITIES ============

int findTrackerIndexByFilename(const char *filename) {
  if (filename == NULL || *filename == '\0') {
    return -1;
  }

  int trackerCount = atomic_load(&numTrackerFiles_g);
  if (trackerCount > MAX_TRACKER_FILES) {
    trackerCount = MAX_TRACKER_FILES;
  }

  for (int i = 0; i < trackerCount; i++) {
    if (trackerArray_g[i].filename[0] == '\0') {
      continue;
    }
    if (strcmp(trackerArray_g[i].filename, filename) == 0) {
      return i;
    }
  }

  return -1;
}

int findPeerIndex(const TrackerInfo *tracker, const char *ip, uint16_t port) {
  if (tracker == NULL || ip == NULL) {
    return -1;
  }

  for (size_t i = 0; i < tracker->numPeers; i++) {
    if (strcmp(tracker->Peers[i].ip, ip) == 0 &&
        tracker->Peers[i].port == port) {
      return (int)i;
    }
  }

  return -1;
}

// ============ COMMAND HANDLERS ============

CommandOutput trackerCreateCommand(const CreateTrackerArgs *args) {
  CommandOutput result = {.status = STATUS_FAIL, .trackerPtr = NULL};
  result.outMsg[0] = '\0';

  if (args == NULL) {
    snprintf(result.outMsg, BUFFER_SIZE, "createtracker: invalid arguments\n");
    return result;
  }

  if (mtx_lock(&trkMutex) != thrd_success) {
    snprintf(result.outMsg, BUFFER_SIZE,
             "createtracker: failed to lock mutex\n");
    return result;
  }

  // Check if tracker already exists
  if (findTrackerIndexByFilename(args->filename) >= 0) {
    snprintf(result.outMsg, BUFFER_SIZE,
             "createtracker: tracker for '%s' already exists\n",
             args->filename);
    mtx_unlock(&trkMutex);
    result.status = STATUS_FILE_ERROR;
    return result;
  }

  // Get next tracker ID
  int trackerId = atomic_load(&numTrackerFiles_g);
  if (trackerId >= MAX_TRACKER_FILES) {
    snprintf(result.outMsg, BUFFER_SIZE, "createtracker: tracker array full\n");
    mtx_unlock(&trkMutex);
    return result;
  }

  // Create tracker in array
  TrackerInfo *tracker = &trackerArray_g[trackerId];

  strncpy(tracker->filename, args->filename, sizeof(tracker->filename) - 1);
  tracker->filename[sizeof(tracker->filename) - 1] = '\0';

  strncpy(tracker->description, args->description,
          sizeof(tracker->description) - 1);
  tracker->description[sizeof(tracker->description) - 1] = '\0';

  strncpy(tracker->md5Hash, args->md5, sizeof(tracker->md5Hash) - 1);
  tracker->md5Hash[sizeof(tracker->md5Hash) - 1] = '\0';

  tracker->filesize = args->filesize;
  tracker->trackerId = trackerId;
  tracker->numPeers = 1;

  // Add initial peer
  strncpy(tracker->Peers[0].ip, args->ip, sizeof(tracker->Peers[0].ip) - 1);
  tracker->Peers[0].ip[sizeof(tracker->Peers[0].ip) - 1] = '\0';

  tracker->Peers[0].port = args->port;
  tracker->Peers[0].startByte = 0;
  tracker->Peers[0].endByte = args->filesize;
  tracker->Peers[0].timestamp = time(NULL);

  atomic_fetch_add(&numTrackerFiles_g, 1);

  result.status = STATUS_OK;
  result.trackerPtr = tracker;
  snprintf(result.outMsg, BUFFER_SIZE,
           "createtracker: created trackerId=%d for file '%s'\n", trackerId,
           args->filename);

  mtx_unlock(&trkMutex);
  return result;
}

CommandOutput trackerUpdateCommand(const UpdateTrackerArgs *args) {
  CommandOutput result = {.status = STATUS_FAIL, .trackerPtr = NULL};
  result.outMsg[0] = '\0';

  if (args == NULL) {
    snprintf(result.outMsg, BUFFER_SIZE, "updatetracker: invalid arguments\n");
    return result;
  }

  if (mtx_lock(&trkMutex) != thrd_success) {
    snprintf(result.outMsg, BUFFER_SIZE,
             "updatetracker: failed to lock mutex\n");
    return result;
  }

  int trackerIdx = findTrackerIndexByFilename(args->filename);
  if (trackerIdx < 0) {
    snprintf(result.outMsg, BUFFER_SIZE,
             "updatetracker: tracker for '%s' not found\n", args->filename);
    mtx_unlock(&trkMutex);
    result.status = STATUS_FILE_ERROR;
    return result;
  }

  TrackerInfo *tracker = &trackerArray_g[trackerIdx];

  int peerIdx = findPeerIndex(tracker, args->ip, args->port);

  if (peerIdx >= 0) {
    // Update existing peer
    tracker->Peers[peerIdx].startByte = args->startByte;
    tracker->Peers[peerIdx].endByte = args->endByte;
    tracker->Peers[peerIdx].timestamp = time(NULL);

    snprintf(result.outMsg, BUFFER_SIZE,
             "updatetracker: updated existing peer for '%s'\n", args->filename);
  } else {
    // Add new peer
    if (tracker->numPeers >= MAX_PEERS) {
      snprintf(result.outMsg, BUFFER_SIZE,
               "updatetracker: max peers reached for '%s'\n", args->filename);
      mtx_unlock(&trkMutex);
      return result;
    }

    PeerInfo *peer = &tracker->Peers[tracker->numPeers];
    strncpy(peer->ip, args->ip, sizeof(peer->ip) - 1);
    peer->ip[sizeof(peer->ip) - 1] = '\0';

    peer->port = args->port;
    peer->startByte = args->startByte;
    peer->endByte = args->endByte;
    peer->timestamp = time(NULL);

    tracker->numPeers++;

    snprintf(result.outMsg, BUFFER_SIZE,
             "updatetracker: added new peer for '%s'\n", args->filename);
  }

  result.status = STATUS_OK;
  result.trackerPtr = tracker;

  mtx_unlock(&trkMutex);
  return result;
}

CommandOutput trackerListCommand(const char *arg) {
  (void)arg; // Unused
  CommandOutput result = {.status = STATUS_FAIL, .trackerPtr = NULL};
  result.outMsg[0] = '\0';

  if (mtx_lock(&trkMutex) != thrd_success) {
    snprintf(result.outMsg, BUFFER_SIZE, "list: failed to lock mutex\n");
    return result;
  }

  int trackerCount = atomic_load(&numTrackerFiles_g);
  if (trackerCount > MAX_TRACKER_FILES) {
    trackerCount = MAX_TRACKER_FILES;
  }

  size_t validCount = 0;
  for (int i = 0; i < trackerCount; i++) {
    if (trackerArray_g[i].filename[0] != '\0') {
      validCount++;
    }
  }

  size_t len = 0;

  len += snprintf(result.outMsg + len, BUFFER_SIZE - len, "REP LIST %zu\n",
                  validCount);

  size_t displayIndex = 1;
  for (int i = 0; i < trackerCount; i++) {
    if (trackerArray_g[i].filename[0] == '\0') {
      continue;
    }

    len += snprintf(result.outMsg + len, BUFFER_SIZE - len,
                    "%zu %s %zu %s [trackerId=%zu]\n", displayIndex,
                    trackerArray_g[i].filename, trackerArray_g[i].filesize,
                    trackerArray_g[i].md5Hash, trackerArray_g[i].trackerId);

    if (len >= BUFFER_SIZE) {
      // Prevent overflow
      break;
    }

    displayIndex++;
  }

  if (len < BUFFER_SIZE) {
    snprintf(result.outMsg + len, BUFFER_SIZE - len, "REP LIST END\n");
  }

  result.status = STATUS_OK;
  mtx_unlock(&trkMutex);
  return result;
}

CommandOutput trackerGetCommand(const GetTrackerArgs *args) {
  CommandOutput result = {.status = STATUS_FAIL, .trackerPtr = NULL};
  result.outMsg[0] = '\0';

  if (args == NULL) {
    snprintf(result.outMsg, BUFFER_SIZE, "get: invalid arguments\n");
    return result;
  }

  if (mtx_lock(&trkMutex) != thrd_success) {
    snprintf(result.outMsg, BUFFER_SIZE, "get: failed to lock mutex\n");
    return result;
  }

  TrackerInfo *tracker = NULL;

  if (args->isId) {
    long trackerId = strtol(args->query, NULL, 10);
    if (trackerId >= 0 && trackerId < MAX_TRACKER_FILES &&
        trackerArray_g[trackerId].filename[0] != '\0') {
      tracker = &trackerArray_g[trackerId];
    }
  } else {
    int trackerIdx = findTrackerIndexByFilename(args->query);
    if (trackerIdx >= 0) {
      tracker = &trackerArray_g[trackerIdx];
    }
  }

  if (tracker == NULL) {
    snprintf(result.outMsg, BUFFER_SIZE, "get: tracker not found for '%s'\n",
             args->query);
    mtx_unlock(&trkMutex);
    result.status = STATUS_FILE_ERROR;
    return result;
  }

  size_t len = 0;

  len += snprintf(result.outMsg + len, BUFFER_SIZE - len, "REP GET BEGIN\n");

  len += snprintf(result.outMsg + len, BUFFER_SIZE - len, "Filename: %s\n",
                  tracker->filename);

  len += snprintf(result.outMsg + len, BUFFER_SIZE - len, "Filesize: %zu\n",
                  tracker->filesize);

  len += snprintf(result.outMsg + len, BUFFER_SIZE - len, "Description: %s\n",
                  tracker->description);

  len += snprintf(result.outMsg + len, BUFFER_SIZE - len, "MD5: %s\n",
                  tracker->md5Hash);

  for (size_t i = 0; i < tracker->numPeers; i++) {
    len +=
        snprintf(result.outMsg + len, BUFFER_SIZE - len, "%s:%u:%zu:%zu:%ld\n",
                 tracker->Peers[i].ip, tracker->Peers[i].port,
                 tracker->Peers[i].startByte, tracker->Peers[i].endByte,
                 (long)tracker->Peers[i].timestamp);

    if (len >= BUFFER_SIZE) {
      break;
    }
  }

  if (len < BUFFER_SIZE) {
    snprintf(result.outMsg + len, BUFFER_SIZE - len, "REP GET END %s\n",
             tracker->md5Hash);
  }

  result.status = STATUS_OK;
  result.trackerPtr = tracker;

  mtx_unlock(&trkMutex);
  return result;
}

CommandOutput trackerExitCommand(const char *arg) {
  (void)arg; // Unused
  printf("Peer Exited!\n");
  CommandOutput result = {.status = STATUS_EXIT, .trackerPtr = NULL};
  result.outMsg[0] = '\0';
  return result;
}

// ============ CLIENT MESSAGE HANDLING ============

static void receiveClientMsgs(int32_t clientSocketFD) {
  char buffer[BUFFER_SIZE];
  while (true) {
    size_t recvMsgSize = recvSocket(clientSocketFD, buffer, sizeof(buffer), 0);

    if (recvMsgSize <= 0) {
      break;
    }

    buffer[strcspn(buffer, "\n")] = 0;

    // PURE PARSING - no handler execution
    ParsedCommand parsed = parseCommand(buffer);

    if (!parsed.parseSuccess) {
      sendSocket(clientSocketFD, parsed.parseError, sizeof(parsed.parseError),
                 0);
      continue;
    }

    // DISPATCH to appropriate handler
    CommandOutput result;
    memset(&result, 0, sizeof(result));

    switch (parsed.type) {
    case CMD_CREATE_TRACKER:
      result = trackerCreateCommand(&parsed.args.createTracker);
      break;
    case CMD_UPDATE_TRACKER:
      result = trackerUpdateCommand(&parsed.args.updateTracker);
      break;
    case CMD_LIST:
      result = trackerListCommand(NULL);
      break;
    case CMD_GET:
      result = trackerGetCommand(&parsed.args.getTracker);
      break;
    case CMD_EXIT:
      result = trackerExitCommand(NULL);
      break;
    default:
      snprintf(result.outMsg, BUFFER_SIZE, "Unknown command\n");
      result.status = STATUS_FAIL;
      break;
    }

    // Send response
    if (result.status != STATUS_EXIT) {
      // Send response message first (always sent, whether success or failure)
      sendSocket(clientSocketFD, result.outMsg, BUFFER_SIZE, 0);
      
      // Send file only if GET was successful (file exists and was retrieved)
      if (parsed.type == CMD_GET && result.trackerPtr != NULL) {
        getAndSendTrackerInfo(result.trackerPtr, clientSocketFD);
      }

      // File operations if needed (for CREATE/UPDATE)
      if (result.trackerPtr != NULL) {
        if (parsed.type == CMD_CREATE_TRACKER) {
          createTrackerFile(result.trackerPtr->trackerId);
        } else if (parsed.type == CMD_UPDATE_TRACKER) {
          updateTrackerFile(result.trackerPtr->trackerId);
        }
      }
    } else {
      // Send exit message before breaking
      sendSocket(clientSocketFD, result.outMsg, BUFFER_SIZE, 0);
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

// ============ FILE OPERATIONS ============

CommandStatus createTrackerFile(size_t trackerId) {
  TrackerInfo *tr;
  if (mtx_lock(&trkMutex) != thrd_success) {
    return STATUS_FAIL;
  }
  tr = &trackerArray_g[trackerId];

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
            (long)tr->Peers[i].timestamp);
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
            (long)tr->Peers[i].timestamp);
  }

  fclose(tFile);
  mtx_unlock(&trkMutex);

  return STATUS_OK;
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
  // testCreateAndUpdateTracker();

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

    // getAndSendTrackerInfo(&trackerArray_g[1], clientSocketFD);
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
