#pragma once

#include "protocol.h"
#include <stdint.h>
#include <stdio.h>
#include <threads.h>

// Represents a single file segment
typedef struct {
  uint32_t segment_id; // Segment identifier
  uint32_t offset;     // Byte offset in file
  uint32_t size;       // Segment size in bytes
  int downloaded;      // Flag: 1 if downloaded, 0 otherwise
} FileSegment;

// Represents the state of a file being downloaded
typedef struct {
  char filename[MAX_FILENAME_LEN];
  uint32_t total_size;   // Total file size in bytes
  uint32_t segment_size; // Size of each segment
  uint32_t num_segments; // Total number of segments
  FileSegment *segments; // Array of segments
  int record_updated;    // Track if record file needs update
  FILE *file_handle;     // File pointer for writing
  mtx_t lock;            // Mutex for thread-safe access
} FileDownloadState;

// Represents all peers in the swarm
typedef struct {
  PeerInfo peers[MAX_PEERS];
  uint32_t num_peers;
  mtx_t lock; // Mutex for thread-safe access
} PeerSwarm;

// Function Declarations

CommandStatus get_peer_config();

CommandType get_peer_command(char *line, int32_t socketFD);

// Initialization functions
void peer_init(void);
void peer_cleanup(void);

// File segment management
void create_file_segments(FileDownloadState *state, const char *filename,
                          uint32_t segment_size);
void free_file_segments(FileDownloadState *state);

// Segment selection - select next undownloaded segment
FileSegment *select_next_segment(FileDownloadState *state);

// Peer selection - find peer with newest timestamp that has the segment
PeerInfo *select_peer_for_segment(PeerSwarm *swarm, uint32_t segment_id);

// Download operations
int download_segment(PeerInfo *peer, FileSegment *segment,
                     FileDownloadState *state);

// Record update - update local record and notify tracker
int update_segment_record(FileDownloadState *state, uint32_t segment_id,
                          const char *tracker_ip, uint16_t tracker_port);

// Tracker communication
int notify_tracker(const char *tracker_ip, uint16_t tracker_port,
                   const char *filename, uint32_t segment_id);
int get_peer_list_from_tracker(const char *tracker_ip, uint16_t tracker_port,
                               const char *filename, PeerSwarm *swarm);

// Thread functions
int download_thread_worker(void *arg);
int tracker_sync_thread_worker(void *arg);

// Peer list management
void add_peer(PeerSwarm *swarm, const char *ip, uint16_t port);
void remove_peer(PeerSwarm *swarm, uint32_t peer_index);
void update_peer_timestamp(PeerSwarm *swarm, uint32_t peer_index);
PeerInfo *get_peer_with_newest_timestamp(PeerSwarm *swarm, uint32_t segment_id);

// Utility functions
uint32_t calculate_num_segments(uint32_t file_size, uint32_t segment_size);
void print_download_progress(FileDownloadState *state);
