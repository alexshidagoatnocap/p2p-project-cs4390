#pragma once

/*
    protocol.h

    Shared protocol definitions for tracker.c and peer.c.
*/

#include <stddef.h>
#include <stdint.h>

#define BUFFER_SIZE 4096
#define CHUNK_SIZE 1024

typedef enum {
    CMD_UNKNOWN = 0,
    CMD_CREATE_TRACKER,
    CMD_UPDATE_TRACKER,
    CMD_LIST,
    CMD_GET,
    CMD_EXIT
} CommandType;

typedef enum {
    STATUS_OK = 0,
    STATUS_FAIL,
    STATUS_FILE_ERROR,
    STATUS_EXIT
} CommandStatus;

typedef struct {
    CommandType Type;
    CommandStatus Status;

    char rawLine[BUFFER_SIZE];

    char filename[256];
    size_t filesize;
    char description[256];
    char md5Hash[33];

    char ip[64];
    uint16_t port;

    uint32_t startByte;
    uint32_t endByte;
} CommandInfo;

typedef struct {
    char filename[256];
    size_t filesize;
    char description[256];
    char md5Hash[33];
} TrackerInfo;

extern TrackerInfo trackerArray_g[];

CommandInfo parseCommand(const char *line);
const char *commandTypeToString(CommandType type);
const char *commandStatusToString(CommandStatus status);
