#pragma once

// Standard libraries for types and threading
#include <stdatomic.h> // atomic operations
#include <stddef.h>    // size_t
#include <stdint.h>    // uint16_t
#include <threads.h>   // mutex (mtx_t)
#include <time.h>      // timestamps

constexpr int32_t BUFFER_SIZE = 2048;
constexpr int32_t CHUNK_SIZE = 1024;
constexpr int32_t MAX_PEERS = 256;
constexpr int32_t MAX_TRACKER_FILES = 256;
constexpr int32_t TRK_FNAME_SIZE = 512;

// Enum for command types
typedef enum {
  CMD_UNKNOWN = 0,
  CMD_CREATE_TRACKER,
  CMD_UPDATE_TRACKER,
  CMD_LIST,
  CMD_GET,
  CMD_EXIT
} CommandType;

// Enum for command result status
typedef enum {
  STATUS_OK = 0,
  STATUS_FAIL,
  STATUS_FILE_ERROR,
  STATUS_EXIT
} CommandStatus;

// Information about a peer that has part of a file
typedef struct {
  char ip[16];      // IP address (e.g., "127.0.0.1")
  uint16_t port;    // port number
  size_t startByte; // starting byte of chunk
  size_t endByte;   // ending byte of chunk
  time_t timestamp; // last update time
} PeerInfo;

// Information about a file being tracked
typedef struct {
  char filename[256];        // file name
  size_t filesize;           // file size
  char description[256];     // description (can be multi-word)
  char md5Hash[33];          // MD5 checksum
  PeerInfo Peers[MAX_PEERS]; // list of peers
  size_t numPeers;           // number of peers
  size_t trackerId;          // unique ID assigned by tracker
} TrackerInfo;

// Output from a command
typedef struct {
  CommandStatus Status;    // success or failure
  TrackerInfo *TrackerPtr; // pointer used mainly for GET
  char outMsg[BUFFER_SIZE];
} CommandOutput;

// Full parsed command info
typedef struct {
  CommandType Type;
  CommandOutput Output;
} CommandInfo;

// These are defined in tracker.c, but used here
extern TrackerInfo trackerArray_g[MAX_TRACKER_FILES];
extern atomic_int numTrackerFiles_g;
extern mtx_t trkMutex;

// Function pointer type for handlers
typedef CommandOutput (*CommandHandler)(const char *arg);

// Main parsing function
CommandInfo parseCommand(char *line);

CommandType identifyCommand(char *command);
