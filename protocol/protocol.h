#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

constexpr int32_t BUFFER_SIZE = 4096;
constexpr int32_t CHUNK_SIZE = 1024;
constexpr int32_t MAX_PEERS = 256;
constexpr int32_t MAX_TRACKER_FILES = 256;
constexpr int32_t TRK_FNAME_SIZE = 512;

typedef enum {
  CMD_UNKNOWN,
  CMD_CREATE_TRACKER,
  CMD_UPDATE_TRACKER,
  CMD_LIST,
  CMD_GET,
  CMD_EXIT,
} CommandType;

typedef enum {
  STATUS_OK,
  STATUS_FAIL,
  STATUS_FILE_ERROR,
  STATUS_EXIT
} CommandStatus;

typedef struct {
  char ip[16];
  uint16_t port;
  size_t startByte;
  size_t endByte;
  time_t timestamp;
} PeerInfo;

typedef struct {
  char filename[256];
  size_t filesize;
  char description[256];
  char md5Hash[33];
  PeerInfo Peers[MAX_PEERS];
  size_t numPeers;
  size_t trackerId;
} TrackerInfo;

typedef struct {
  CommandStatus Status;
  TrackerInfo *TrackerPtr;
} CommandOutput;

typedef struct {
  CommandType Type;
  CommandOutput Output;
} CommandInfo;

extern TrackerInfo trackerArray_g[MAX_TRACKER_FILES];

CommandInfo parseCommand(char *line);

typedef CommandOutput (*CommandHandler)(const char *arg);
