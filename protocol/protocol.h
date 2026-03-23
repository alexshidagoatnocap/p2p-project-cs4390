#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint32_t BUFFER_SIZE = 4096;
constexpr uint32_t CHUNK_SIZE = 1024;

typedef enum {
  CMD_UNKNOWN,
  CMD_LIST,
  CMD_GET,
  CMD_CREATE_TRACKER,
  CMD_UPDATE_TRACKER,
} CommandType;

typedef enum { STATUS_OK, STATUS_FAIL, STATUS_FILE_ERROR } CommandStatus;

typedef struct {
  char filename[256];
  size_t filesize;
  char description[256];
  char md5Hash[33];
  char ip[64];
  uint32_t port;
} CreateTrackerMailbox;

typedef struct {
  char filename[256];
  size_t startBytes;
  size_t endBytes;
  char ip[64];
  uint32_t port;
} UpdateTrackerMailbox;

CommandType parseCommand(const char *line);
CommandStatus createTracker(const char *line, CreateTrackerMailbox *ct);
CommandStatus updateTracker(const char *line, UpdateTrackerMailbox *ut);
CommandStatus list();
CommandStatus get(const char *filename);
