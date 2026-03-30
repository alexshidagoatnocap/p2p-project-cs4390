#pragma once

#include "protocol.h"

// ============ COMMAND OUTPUT ============
typedef struct {
  CommandStatus status;
  TrackerInfo *trackerPtr;
  char outMsg[BUFFER_SIZE];
} CommandOutput;

// ============ COMMAND HANDLERS ============
// These execute tracker business logic and manipulate tracker state

CommandOutput trackerCreateCommand(const CreateTrackerArgs *args);
CommandOutput trackerUpdateCommand(const UpdateTrackerArgs *args);
CommandOutput trackerListCommand(const char *arg);
CommandOutput trackerGetCommand(const GetTrackerArgs *args);
CommandOutput trackerExitCommand(const char *arg);

// ============ TRACKER LOOKUP UTILITIES ============
int findTrackerIndexByFilename(const char *filename);
int findPeerIndex(const TrackerInfo *tracker, const char *ip, uint16_t port);

// ============ FILE OPERATIONS ============
CommandStatus getAndSendTrackerInfo(TrackerInfo *tr, int32_t peerSockFD);
CommandStatus createTrackerFile(size_t trackerId);
CommandStatus updateTrackerFile(size_t trackerId);

// ============ STATE GLOBALS ============
extern TrackerInfo trackerArray_g[MAX_TRACKER_FILES];
extern atomic_int numTrackerFiles_g;
extern mtx_t trkMutex;
