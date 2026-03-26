#pragma once

// Standard libraries for types and threading
#include <stddef.h>     // size_t
#include <stdint.h>     // uint16_t
#include <stdatomic.h>  // atomic operations
#include <threads.h>    // mutex (mtx_t)
#include <time.h>       // timestamps

// Constants (C-compatible way instead of constexpr)
enum {
    BUFFER_SIZE = 4096,          // buffer for reading commands
    CHUNK_SIZE = 1024,           // chunk size for file transfer
    MAX_PEERS = 256,             // max peers per file
    MAX_TRACKER_FILES = 256      // max files tracked
};

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
    char ip[16];            // IP address (e.g., "127.0.0.1")
    uint16_t port;          // port number
    size_t startByte;       // starting byte of chunk
    size_t endByte;         // ending byte of chunk
    time_t timestamp;       // last update time
} PeerInfo;

// Information about a file being tracked
typedef struct {
    char filename[256];     // file name
    size_t filesize;        // file size
    char description[256];  // description (can be multi-word)
    char md5Hash[33];       // MD5 checksum
    PeerInfo Peers[MAX_PEERS]; // list of peers
    size_t numPeers;        // number of peers
    size_t trackerId;       // unique ID assigned by tracker
} TrackerInfo;

// Output from a command
typedef struct {
    CommandStatus Status;       // success or failure
    TrackerInfo *TrackerPtr;    // pointer used mainly for GET
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


/* #pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

constexpr int32_t BUFFER_SIZE = 4096;
constexpr int32_t CHUNK_SIZE = 1024;
constexpr int32_t MAX_PEERS = 256;
constexpr int32_t MAX_TRACKER_FILES = 256;

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


*/
