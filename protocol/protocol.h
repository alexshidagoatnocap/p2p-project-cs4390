#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint32_t BUFFER_SIZE = 4096;
constexpr uint32_t CHUNK_SIZE = 1024;

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
  char filename[256];
  size_t filesize;
  char description[256];
  char md5Hash[33];
} TrackerInfo;

extern TrackerInfo trackerArray_g[];

CommandInfo parseCommand(char *line);

typedef CommandStatus (*CommandHandler)(const char *arg);
