#include "protocol.h"
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// FIX: PUT THESE IN TRACKER! HERE FOR TESTING ONLY
TrackerInfo trackerArray_g[MAX_TRACKER_FILES];
atomic_int numTrackerFiles_g = 0;
mtx_t trkMutex;

static void trimWhitespaceInPlace(char *str) {
  if (str == NULL || *str == '\0') {
    return;
  }

  char *start = str;
  while (*start && isspace((unsigned char)*start)) {
    start++;
  }

  if (start != str) {
    memmove(str, start, strlen(start) + 1);
  }

  size_t len = strlen(str);
  while (len > 0 && isspace((unsigned char)str[len - 1])) {
    str[len - 1] = '\0';
    len--;
  }
}

static void stripOptionalAngleBrackets(char *str) {
  if (str == NULL) {
    return;
  }

  trimWhitespaceInPlace(str);
  size_t len = strlen(str);

  if (len > 0 && str[0] == '<') {
    memmove(str, str + 1, len);
  }

  len = strlen(str);
  if (len > 0 && str[len - 1] == '>') {
    str[len - 1] = '\0';
  }

  trimWhitespaceInPlace(str);
}

static int stringsEqualIgnoreCase(const char *a, const char *b) {
  if (a == NULL || b == NULL) {
    return 0;
  }

  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return 0;
    }
    a++;
    b++;
  }

  return (*a == '\0' && *b == '\0');
}

static int isNumberString(const char *s) {
  if (s == NULL || *s == '\0') {
    return 0;
  }

  while (*s) {
    if (!isdigit((unsigned char)*s)) {
      return 0;
    }
    s++;
  }
  return 1;
}

static void safeStringCopy(char *dest, size_t destSize, const char *src) {
  if (dest == NULL || destSize == 0) {
    return;
  }

  if (src == NULL) {
    dest[0] = '\0';
    return;
  }

  snprintf(dest, destSize, "%s", src);
}

static void removeTrackSuffix(const char *input, char *output,
                              size_t outputSize) {
  if (output == NULL || outputSize == 0) {
    return;
  }

  output[0] = '\0';

  if (input == NULL) {
    return;
  }

  safeStringCopy(output, outputSize, input);

  size_t len = strlen(output);
  if (len >= 6) {
    const char *suffix = ".track";
    size_t suffixLen = strlen(suffix);
    if (len >= suffixLen && strcmp(output + len - suffixLen, suffix) == 0) {
      output[len - suffixLen] = '\0';
    }
  }

  len = strlen(output);
  if (len >= 4) {
    const char *suffix = ".trk";
    size_t suffixLen = strlen(suffix);
    if (len >= suffixLen && strcmp(output + len - suffixLen, suffix) == 0) {
      output[len - suffixLen] = '\0';
    }
  }
}

static int findTrackerIndexByFilenameUnlocked(const char *filename) {
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

static int findPeerIndexUnlocked(const TrackerInfo *tracker, const char *ip,
                                 uint16_t port) {
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

static size_t splitTokens(char *buffer, char *tokens[], size_t maxTokens) {
  size_t count = 0;
  char *savePtr = NULL;
  char *tok = strtok_r(buffer, " \t\r\n", &savePtr);

  while (tok != NULL && count < maxTokens) {
    tokens[count++] = tok;
    tok = strtok_r(NULL, " \t\r\n", &savePtr);
  }

  return count;
}

static CommandOutput createTrackerHandler(const char *arg) {
  /*
   * Expected general format:
   * createtracker filename filesize description md5 ip port
   *
   * If description contains spaces, this handler supports that by treating:
   * token[0] = filename
   * token[1] = filesize
   * token[last-3] = md5
   * token[last-2] = ip
   * token[last-1] = port
   * description = everything in between
   */

  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};

  if (arg == NULL) {
    printf("createtracker: missing arguments\n");
    return result;
  }

  char buffer[BUFFER_SIZE];
  safeStringCopy(buffer, sizeof(buffer), arg);
  trimWhitespaceInPlace(buffer);

  char *tokens[64];
  size_t tokenCount = splitTokens(buffer, tokens, 64);

  if (tokenCount < 6) {
    printf("createtracker: not enough arguments\n");
    return result;
  }

  const char *filename = tokens[0];
  const char *filesizeStr = tokens[1];
  const char *md5 = tokens[tokenCount - 3];
  const char *ip = tokens[tokenCount - 2];
  const char *portStr = tokens[tokenCount - 1];

  if (!isNumberString(filesizeStr) || !isNumberString(portStr)) {
    printf("createtracker: filesize/port must be numeric\n");
    return result;
  }

  char description[256] = {0};
  size_t descPos = 0;
  for (size_t i = 2; i < tokenCount - 3; i++) {
    int written = snprintf(description + descPos, sizeof(description) - descPos,
                           "%s%s", (i == 2 ? "" : " "), tokens[i]);
    if (written < 0 || (size_t)written >= sizeof(description) - descPos) {
      printf("createtracker: description too long\n");
      return result;
    }
    descPos += (size_t)written;
  }

  size_t filesize = (size_t)strtoull(filesizeStr, NULL, 10);
  unsigned long parsedPort = strtoul(portStr, NULL, 10);

  if (parsedPort > 65535UL) {
    printf("createtracker: invalid port\n");
    return result;
  }

  if (mtx_lock(&trkMutex) != thrd_success) {
    printf("createtracker: failed to lock tracker mutex\n");
    return result;
  }

  if (findTrackerIndexByFilenameUnlocked(filename) >= 0) {
    printf("createtracker: tracker for '%s' already exists\n", filename);
    mtx_unlock(&trkMutex);
    result.Status = STATUS_FILE_ERROR;
    return result;
  }

  int trackerId = atomic_fetch_add(&numTrackerFiles_g, 1);
  if (trackerId < 0 || trackerId >= MAX_TRACKER_FILES) {
    printf("createtracker: tracker array full\n");
    atomic_fetch_sub(&numTrackerFiles_g, 1);
    mtx_unlock(&trkMutex);
    return result;
  }

  TrackerInfo tracker = {0};
  safeStringCopy(tracker.filename, sizeof(tracker.filename), filename);
  safeStringCopy(tracker.description, sizeof(tracker.description), description);
  safeStringCopy(tracker.md5Hash, sizeof(tracker.md5Hash), md5);
  tracker.filesize = filesize;
  tracker.numPeers = 1;
  tracker.trackerId = (size_t)trackerId;

  safeStringCopy(tracker.Peers[0].ip, sizeof(tracker.Peers[0].ip), ip);
  tracker.Peers[0].port = (uint16_t)parsedPort;
  tracker.Peers[0].startByte = 0;
  tracker.Peers[0].endByte = filesize;
  tracker.Peers[0].timestamp = time(NULL);

  trackerArray_g[trackerId] = tracker;
  result.Status = STATUS_OK;
  result.TrackerPtr = &trackerArray_g[trackerId];

  mtx_unlock(&trkMutex);

  printf("createtracker: created trackerId=%d for file '%s'\n", trackerId,
         filename);

  return result;
}

static CommandOutput updateTrackerHandler(const char *arg) {
  /*
   * Expected format:
   * updatetracker filename startByte endByte ip port
   */

  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};

  if (arg == NULL) {
    printf("updatetracker: missing arguments\n");
    return result;
  }

  char buffer[BUFFER_SIZE];
  safeStringCopy(buffer, sizeof(buffer), arg);
  trimWhitespaceInPlace(buffer);

  char *tokens[16];
  size_t tokenCount = splitTokens(buffer, tokens, 16);

  if (tokenCount != 5) {
    printf("updatetracker: expected 5 arguments\n");
    return result;
  }

  const char *filename = tokens[0];
  const char *startStr = tokens[1];
  const char *endStr = tokens[2];
  const char *ip = tokens[3];
  const char *portStr = tokens[4];

  if (!isNumberString(startStr) || !isNumberString(endStr) ||
      !isNumberString(portStr)) {
    printf("updatetracker: start/end/port must be numeric\n");
    return result;
  }

  size_t startByte = (size_t)strtoull(startStr, NULL, 10);
  size_t endByte = (size_t)strtoull(endStr, NULL, 10);
  unsigned long parsedPort = strtoul(portStr, NULL, 10);

  if (parsedPort > 65535UL || startByte > endByte) {
    printf("updatetracker: invalid range or port\n");
    return result;
  }

  if (mtx_lock(&trkMutex) != thrd_success) {
    printf("updatetracker: failed to lock tracker mutex\n");
    return result;
  }

  int trackerIdx = findTrackerIndexByFilenameUnlocked(filename);
  if (trackerIdx < 0) {
    printf("updatetracker: tracker for '%s' not found\n", filename);
    mtx_unlock(&trkMutex);
    result.Status = STATUS_FILE_ERROR;
    return result;
  }

  TrackerInfo *tracker = &trackerArray_g[trackerIdx];

  int peerIdx = findPeerIndexUnlocked(tracker, ip, (uint16_t)parsedPort);
  if (peerIdx >= 0) {
    tracker->Peers[peerIdx].startByte = startByte;
    tracker->Peers[peerIdx].endByte = endByte;
    tracker->Peers[peerIdx].timestamp = time(NULL);
    printf("updatetracker: updated existing peer for '%s'\n", filename);
  } else {
    if (tracker->numPeers >= MAX_PEERS) {
      printf("updatetracker: max peers reached for '%s'\n", filename);
      mtx_unlock(&trkMutex);
      return result;
    }

    PeerInfo *peer = &tracker->Peers[tracker->numPeers];
    safeStringCopy(peer->ip, sizeof(peer->ip), ip);
    peer->port = (uint16_t)parsedPort;
    peer->startByte = startByte;
    peer->endByte = endByte;
    peer->timestamp = time(NULL);
    tracker->numPeers++;

    printf("updatetracker: added new peer for '%s'\n", filename);
  }

  result.Status = STATUS_OK;
  result.TrackerPtr = tracker;
  mtx_unlock(&trkMutex);

  return result;
}

static CommandOutput listHandler(const char *arg) {
  (void)arg;

  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};

  if (mtx_lock(&trkMutex) != thrd_success) {
    printf("list: failed to lock tracker mutex\n");
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

  printf("REP LIST %zu\n", validCount);

  size_t displayIndex = 1;
  for (int i = 0; i < trackerCount; i++) {
    if (trackerArray_g[i].filename[0] == '\0') {
      continue;
    }

    printf("%zu %s %zu %s [trackerId=%zu]\n", displayIndex,
           trackerArray_g[i].filename, trackerArray_g[i].filesize,
           trackerArray_g[i].md5Hash, trackerArray_g[i].trackerId);
    displayIndex++;
  }

  printf("REP LIST END\n");

  result.Status = STATUS_OK;
  mtx_unlock(&trkMutex);
  return result;
}

static CommandOutput getHandler(const char *arg) {
  /*
   * Supports:
   * get filename
   * get filename.track
   * get trackerId
   * GET filename.track
   */

  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};

  if (arg == NULL) {
    printf("get: missing argument\n");
    return result;
  }

  char buffer[BUFFER_SIZE];
  safeStringCopy(buffer, sizeof(buffer), arg);
  trimWhitespaceInPlace(buffer);

  if (buffer[0] == '\0') {
    printf("get: missing argument\n");
    return result;
  }

  if (mtx_lock(&trkMutex) != thrd_success) {
    printf("get: failed to lock tracker mutex\n");
    return result;
  }

  TrackerInfo *tracker = NULL;

  if (isNumberString(buffer)) {
    long trackerId = strtol(buffer, NULL, 10);
    if (trackerId >= 0 && trackerId < MAX_TRACKER_FILES &&
        trackerArray_g[trackerId].filename[0] != '\0') {
      tracker = &trackerArray_g[trackerId];
    }
  } else {
    char filenameOnly[256];
    removeTrackSuffix(buffer, filenameOnly, sizeof(filenameOnly));

    int trackerIdx = findTrackerIndexByFilenameUnlocked(filenameOnly);
    if (trackerIdx >= 0) {
      tracker = &trackerArray_g[trackerIdx];
    }
  }

  if (tracker == NULL) {
    printf("get: tracker not found for '%s'\n", buffer);
    mtx_unlock(&trkMutex);
    result.Status = STATUS_FILE_ERROR;
    return result;
  }

  printf("REP GET BEGIN\n");
  printf("Filename: %s\n", tracker->filename);
  printf("Filesize: %zu\n", tracker->filesize);
  printf("Description: %s\n", tracker->description);
  printf("MD5: %s\n", tracker->md5Hash);

  for (size_t i = 0; i < tracker->numPeers; i++) {
    printf("%s:%u:%zu:%zu:%ld\n", tracker->Peers[i].ip, tracker->Peers[i].port,
           tracker->Peers[i].startByte, tracker->Peers[i].endByte,
           (long)tracker->Peers[i].timestamp);
  }

  printf("REP GET END %s\n", tracker->md5Hash);

  result.Status = STATUS_OK;
  result.TrackerPtr = tracker;
  mtx_unlock(&trkMutex);

  return result;
}

static CommandOutput exitHandler(const char *line) {
  (void)line;
  printf("Peer Exited!\n");
  CommandOutput result = {.Status = STATUS_EXIT, .TrackerPtr = NULL};
  return result;
}

static CommandOutput unknownHandler(const char *line) {
  (void)line;
  perror("Unknown Command!\n");
  printf("Unknown Command!\n");
  CommandOutput result = {.Status = STATUS_FAIL, .TrackerPtr = NULL};
  return result;
}

static CommandType identifyCommand(char *command) {
  if (command == NULL) {
    return CMD_UNKNOWN;
  }

  if (stringsEqualIgnoreCase(command, "createtracker")) {
    return CMD_CREATE_TRACKER;
  } else if (stringsEqualIgnoreCase(command, "updatetracker")) {
    return CMD_UPDATE_TRACKER;
  } else if (stringsEqualIgnoreCase(command, "list")) {
    return CMD_LIST;
  } else if (stringsEqualIgnoreCase(command, "get")) {
    return CMD_GET;
  } else if (stringsEqualIgnoreCase(command, "exit")) {
    return CMD_EXIT;
  }

  return CMD_UNKNOWN;
}

static CommandHandler commands[] = {unknownHandler,       createTrackerHandler,
                                    updateTrackerHandler, listHandler,
                                    getHandler,           exitHandler};

CommandInfo parseCommand(char *line) {
  CommandInfo info = {.Type = CMD_UNKNOWN,
                      .Output = {.Status = STATUS_FAIL, .TrackerPtr = NULL}};

  // stripOptionalAngleBrackets(line);

  if (line[0] == '\0') {
    printf("parseCommand: empty input\n");
    return info;
  }

  char *savePtr = NULL;
  char *commandStr = strtok_r(line, " \t\r\n", &savePtr);

  if (commandStr == NULL) {
    printf("parseCommand: no command given\n");
    return info;
  }

  /*
   * Support PDF-style "REQ LIST"
   */
  if (stringsEqualIgnoreCase(commandStr, "REQ")) {
    char *subCommand = strtok_r(NULL, " \t\r\n", &savePtr);
    if (subCommand != NULL && stringsEqualIgnoreCase(subCommand, "LIST")) {
      CommandOutput out = listHandler(savePtr);
      info.Type = CMD_LIST;
      info.Output = out;
      return info;
    }

    printf("parseCommand: unknown REQ command\n");
    return info;
  }

  CommandType cmd = identifyCommand(commandStr);
  CommandOutput out = commands[cmd](savePtr);

  info.Type = cmd;
  info.Output.Status = out.Status;

  if (cmd == CMD_GET || cmd == CMD_CREATE_TRACKER ||
      cmd == CMD_UPDATE_TRACKER) {
    info.Output.TrackerPtr = out.TrackerPtr;
  }

  return info;
}
