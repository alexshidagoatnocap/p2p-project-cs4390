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
constexpr int32_t MAX_FILENAME_LEN = 256;
constexpr int32_t MAX_DESCRIP_LEN = 256;
constexpr int32_t MAX_SEGMENTS = 1000;
constexpr int32_t MAX_IP_LEN = 16;

// Enum for command types
typedef enum {
  CMD_UNKNOWN = 0,
  CMD_CREATE_TRACKER,
  CMD_UPDATE_TRACKER,
  CMD_LIST,
  CMD_GET,
  CMD_EXIT
} CommandType;

// Enum for command result status (used by both protocol and tracker)
typedef enum {
  STATUS_OK = 0,
  STATUS_FAIL,
  STATUS_FILE_ERROR,
  STATUS_EXIT
} CommandStatus;

// Information about a peer that has part of a file
typedef struct {
  char ip_address[MAX_IP_LEN]; // IP address (e.g., "127.0.0.1")
  uint16_t port;               // port number
  size_t startByte;            // starting byte of chunk
  size_t endByte;              // ending byte of chunk
  time_t last_update;          // last update time
  uint32_t segments_available; // Bitmask of available segments
  bool is_active;              // Flag: 1 if peer is active, 0 otherwise
} PeerInfo;

// Information about a file being tracked
typedef struct {
  char filename[MAX_FILENAME_LEN];   // file name
  size_t filesize;                   // file size
  char description[MAX_DESCRIP_LEN]; // description (can be multi-word)
  char md5Hash[33];                  // MD5 checksum
  PeerInfo Peers[MAX_PEERS];         // list of peers
  size_t numPeers;                   // number of peers
  size_t trackerId;                  // unique ID assigned by tracker
} TrackerInfo;

// ============ PARSED COMMAND ARGUMENT STRUCTURES ============

// For CREATE TRACKER: "filename filesize description... md5 ip port"
typedef struct {
  char filename[MAX_FILENAME_LEN];
  size_t filesize;
  char description[MAX_DESCRIP_LEN];
  char md5[33];
  char ip[MAX_IP_LEN];
  uint16_t port;
} CreateTrackerArgs;

// For UPDATE TRACKER: "filename startByte endByte ip port"
typedef struct {
  char filename[MAX_FILENAME_LEN];
  size_t startByte;
  size_t endByte;
  char ip[MAX_IP_LEN];
  uint16_t port;
} UpdateTrackerArgs;

// For GET: "filename_or_id"
typedef struct {
  char query[256];
  int isId; // 1 if numeric ID, 0 if filename
} GetTrackerArgs;

// Union for all command arguments
typedef union {
  CreateTrackerArgs createTracker;
  UpdateTrackerArgs updateTracker;
  GetTrackerArgs getTracker;
} CommandArgs;

// Full parsed command result with only arguments (no handler execution)
typedef struct {
  CommandType type;
  CommandArgs args;
  int parseSuccess;
  char parseError[256];
} ParsedCommand;

// Main parsing function - pure string parsing, no handler execution
ParsedCommand parseCommand(char *line);

CommandType identifyCommand(char *command);
