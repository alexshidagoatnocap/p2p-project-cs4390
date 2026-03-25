#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

constexpr int32_t BUFFER_SIZE = 4096;
constexpr int32_t CHUNK_SIZE = 1024;
constexpr int32_t MAX_PEERS = 256;

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
  CommandType Type;
  CommandStatus Status;
} CommandInfo;

typedef struct {
  char ip[16];
  char port[6];
  size_t startByte;
  size_t endByte;
  time_t timestamp;
} PeerInfo;

typedef struct {
  char filename[256];
  size_t filesize;
  char description[256];
  char md5Hash[33];
  PeerInfo peers[MAX_PEERS];
  size_t numPeers;
} TrackerInfo;

extern TrackerInfo trackerArray_g[];

CommandInfo parseCommand(char *line);

typedef CommandStatus (*CommandHandler)(const char *arg);
