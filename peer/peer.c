// Peer system implementation to manage segements and trakcer communication
#include "peer.h"
#include "api.h"
#include "protocol.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

// Global State
static int api_initialized = 0;

// static CommandStatus recvTrackerFile(int32_t socketFD) {
//   constexpr int32_t MAX_PATH_NAME = MAX_FILENAME_LEN + 13;
//   char tfName[MAX_FILENAME_LEN] = "";
//   char tfPath[MAX_PATH_NAME] = "";
//   recvSocket(socketFD, tfName, MAX_FILENAME_LEN, 0);
//   snprintf(tfPath, sizeof(tfPath), "peer/trk/%s.trk", tfName);
//
//   uint32_t fileSize;
//   uint32_t fileSizeNet;
//   recvSocket(socketFD, &fileSizeNet, sizeof(fileSizeNet), 0);
//   fileSize = netToHostLong(fileSizeNet);
//
//   FILE *tFile = fopen(tfPath, "wb");
//   char fileBuffer[CHUNK_SIZE];
//   uint32_t totalRecv = 0;
//
//   while (totalRecv < fileSize) {
//     size_t bytesRecv = recvSocket(socketFD, fileBuffer, fileSize, 0);
//     if (bytesRecv <= 0)
//       break;
//     fwrite(fileBuffer, 1, bytesRecv, tFile);
//     totalRecv += bytesRecv;
//   }
//
//   fclose(tFile);
//   return STATUS_OK;
// }

// static CommandStatus recvRequestedFile(int32_t socketFD) {
//   constexpr int32_t MAX_PATH_NAME = MAX_FILENAME_LEN + 5;
//   char reqFileName[MAX_FILENAME_LEN] = "";
//   char reqFilePath[MAX_PATH_NAME] = "";
//   recvSocket(socketFD, reqFileName, MAX_FILENAME_LEN, 0);
//   snprintf(reqFilePath, sizeof(reqFilePath), "peer/%s", reqFileName);
//
//   uint32_t fileSizeNet;
//   recvSocket(socketFD, &fileSizeNet, sizeof(fileSizeNet), 0);
//   auto fileSize = netToHostLong(fileSizeNet);
//
//   FILE *tFile = fopen(reqFilePath, "wb");
//   char fileBuffer[CHUNK_SIZE];
//   uint32_t totalRecv = 0;
//
//   // WARN: THIS WILL RECV MORE BYTES THAN IT SHOULD, FIX
//   while (totalRecv < fileSize) {
//     size_t bytesRecv = recvSocket(socketFD, fileBuffer, sizeof(fileBuffer),
//     0); if (bytesRecv <= 0)
//       break;
//     fwrite(fileBuffer, 1, bytesRecv, tFile);
//     totalRecv += bytesRecv;
//   }
//
//   fclose(tFile);
//   return STATUS_OK;
// }

static void sleep_seconds(int seconds) {
  struct timespec ts;
  ts.tv_sec = seconds;
  ts.tv_nsec = 0;
  thrd_sleep(&ts, NULL);
}

// Initialization Functions

// Initialize the peer module and socket API
void peer_init(void) {
  if (!api_initialized) {
    initSocketAPI();
    api_initialized = 1;
  }
  printf("Peer module initialized\n");
}

// Clean up peer module resources

void peer_cleanup(void) {
  if (api_initialized) {
    cleanupSocketAPI();
    api_initialized = 0;
  }
  printf("Peer module cleaned up\n");
}

// File Segment Management
// Calculate the number of segments needed for a file
uint32_t calculate_num_segments(uint32_t file_size, uint32_t segment_size) {
  if (segment_size == 0)
    return 0;
  return (file_size + segment_size - 1) / segment_size;
}

/* Create and initialize file segments for download
Divides file into segments of specified size */
void create_file_segments(FileDownloadState *state, const char *filename,
                          uint32_t segment_size) {
  if (!state || !filename || segment_size == 0) {
    printf("Invalid parameters for create_file_segments\n");
    return;

    strncpy(state->filename, filename, MAX_FILENAME_LEN - 1);
    state->filename[MAX_FILENAME_LEN - 1] = '\0';
    state->segment_size = segment_size;

    // Get file size
    FILE *file = fopen(filename, "rb");
    if (!file) {
      printf("Cannot open file: %s\n", filename);
      return;
    }

    fseek(file, 0, SEEK_END);
    state->total_size = ftell(file);
    fclose(file);

    // Calculate number of segments
    state->num_segments =
        calculate_num_segments(state->total_size, segment_size);

    if (state->num_segments > MAX_SEGMENTS) {
      printf("Too many segments: %u > %u\n", state->num_segments, MAX_SEGMENTS);
      state->num_segments = 0;
      return;
    }

    // Allocate segment array
    state->segments =
        (FileSegment *)malloc(state->num_segments * sizeof(FileSegment));
    if (!state->segments) {
      printf("Failed to allocate memory for segments\n");
      state->num_segments = 0;
      return;
    }

    // Initialize each segment
    for (uint32_t i = 0; i < state->num_segments; i++) {
      state->segments[i].segment_id = i;
      state->segments[i].offset = i * segment_size;
      state->segments[i].size =
          (i == state->num_segments - 1)
              ? (state->total_size - state->segments[i].offset)
              : segment_size;
      state->segments[i].downloaded = 0;
    }

    printf("Created %u segments of size %u bytes for file: %s\n",
           state->num_segments, segment_size, filename);
  }
}

// Free allocated segment memory
void free_file_segments(FileDownloadState *state) {
  if (state && state->segments) {
    free(state->segments);
    state->segments = NULL;
    state->num_segments = 0;
  }
}

// Segment Selection

/* Select the next undownloaded segment sequentially
Returns NULL if all segments downloaded */
FileSegment *select_next_segment(FileDownloadState *state) {
  if (!state || !state->segments) {
    return NULL;
  }

  mtx_lock(&state->lock);

  for (uint32_t i = 0; i < state->num_segments; i++) {
    if (!state->segments[i].downloaded) {
      printf("Selected segment %u (offset: %u, size: %u bytes)\n", i,
             state->segments[i].offset, state->segments[i].size);
      mtx_unlock(&state->lock);
      return &state->segments[i];
    }
  }
  mtx_unlock(&state->lock);
  printf("All segments downloaded\n");
  return NULL;
}

// Peer Management

// Add a peer to the swarm
void add_peer(PeerSwarm *swarm, const char *ip, uint16_t port) {
  if (!swarm || swarm->num_peers >= MAX_PEERS || !ip) {
    return;
  }

  mtx_lock(&swarm->lock);

  PeerInfo *peer = &swarm->peers[swarm->num_peers];
  strncpy(peer->ip_address, ip, MAX_IP_LEN - 1);
  peer->ip_address[MAX_IP_LEN - 1] = '\0';
  peer->port = port;
  peer->last_update = time(NULL);
  peer->segments_available = 0;
  peer->is_active = 1;

  swarm->num_peers++;

  printf("Added peer: %s:%u (Total peers: %u)\n", ip, port, swarm->num_peers);

  mtx_unlock(&swarm->lock);
}

// Remove a peer from the swarm
void remove_peer(PeerSwarm *swarm, uint32_t peer_index) {
  if (!swarm || peer_index >= swarm->num_peers) {
    return;
  }

  mtx_lock(&swarm->lock);

  if (peer_index < swarm->num_peers - 1) {
    memmove(&swarm->peers[peer_index], &swarm->peers[peer_index + 1],
            (swarm->num_peers - peer_index - 1) * sizeof(PeerInfo));
  }
  swarm->num_peers--;

  printf("Number of peers removed %u\n", swarm->num_peers);

  mtx_unlock(&swarm->lock);
}

// Update the timestamp of a peer
void update_peer_timestamp(PeerSwarm *swarm, uint32_t peer_index) {
  if (!swarm || peer_index >= swarm->num_peers) {
    return;
  }

  mtx_lock(&swarm->lock);
  swarm->peers[peer_index].last_update = time(NULL);
  mtx_unlock(&swarm->lock);
}

/* Get the peer with the newest timestamp that has the specified segment
This implements the peer selection strategy */
PeerInfo *select_peer_for_segment(PeerSwarm *swarm, uint32_t segment_id) {
  if (!swarm || swarm->num_peers == 0 || segment_id >= MAX_SEGMENTS) {
    return NULL;
  }

  mtx_lock(&swarm->lock);

  PeerInfo *selected_peer = NULL;
  time_t newest_time = 0;

  // Find the active peer with the newest timestamp that has this segment
  for (uint32_t i = 0; i < swarm->num_peers; i++) {
    PeerInfo *peer = &swarm->peers[i];

    if (!peer->is_active) {
      continue;
    }

    // Check if peer has this segment
    if (!(peer->segments_available & (1U << (segment_id % 32)))) {
      continue;
    }

    // Update if this peer has a newer timestamp
    if (peer->last_update > newest_time) {
      newest_time = peer->last_update;
      selected_peer = peer;
    }
  }

  mtx_unlock(&swarm->lock);

  if (selected_peer) {
    printf("Selected peer %s:%u for segment %u at %ld\n",
           selected_peer->ip_address, selected_peer->port, segment_id,
           selected_peer->last_update);
  }

  return selected_peer;
}

// Get the peer with newest timestamp (alternative implementation)
PeerInfo *get_peer_with_newest_timestamp(PeerSwarm *swarm,
                                         uint32_t segment_id) {
  return select_peer_for_segment(swarm, segment_id);
}

// Download Operations

/* Download a segment from a peer
This is a simplified implementation - in production would handle actual
network transfer with error handling, retries, etc. */
int download_segment(PeerInfo *peer, FileSegment *segment,
                     FileDownloadState *state) {
  if (!peer || !segment || !state) {
    printf("Invalid parameters for download_segment\n");
    return -1;
  }

  printf("Starting download of segment %u from peer %s:%u\n",
         segment->segment_id, peer->ip_address, peer->port);

  /* In a real implementation, this would:
  - Create socket connection to peer
  - Send segment request with file name and segment ID
  - Receive segment data
  - Write to file at proper offset
  - Handle errors and retries */

  int32_t sockfd = createIPV4SockStream();
  if (sockfd == -1) {
    printf("Failed to create socket for segment %u\n", segment->segment_id);
    return -1;
  }

  SocketAddress *addr = createIPV4Addr(peer->ip_address, peer->port);
  if (!addr) {
    printf("Failed to create address for peer\n");
    closeSocket(sockfd);
    return -1;
  }

  if (connectToSocket(sockfd, addr) == -1) {
    printf("Failed to connect to peer %s:%u\n", peer->ip_address, peer->port);
    removeIPV4Addr(addr);
    closeSocket(sockfd);
    return -1;
  }

  printf("Successful segment download %u\n", segment->segment_id);

  removeIPV4Addr(addr);
  closeSocket(sockfd);

  mtx_lock(&state->lock);
  segment->downloaded = 1;
  state->record_updated = 1;
  mtx_unlock(&state->lock);

  return 0;
}

// Record Update and Tracker Communication

/*Notify the tracker that a segment has been downloaded
This updates the tracker's knowledge of what segments this peer has */
int notify_tracker(const char *tracker_ip, uint16_t tracker_port,
                   const char *filename, uint32_t segment_id) {
  if (!tracker_ip || !filename) {
    printf("Invalid parameters for notify_tracker\n");
    return -1;
  }

  printf("Notifying tracker of segment %u for file %s\n", segment_id, filename);

  int32_t sockfd = createIPV4SockStream();
  if (sockfd == -1) {
    printf("Failed to create socket for tracker notification\n");
    return -1;
  }

  SocketAddress *addr = createIPV4Addr(tracker_ip, tracker_port);
  if (!addr) {
    printf("Failed to create tracker address\n");
    closeSocket(sockfd);
    return -1;
  }

  if (connectToSocket(sockfd, addr) == -1) {
    printf("Failed to connect tracker at %s:%u\n", tracker_ip, tracker_port);
    removeIPV4Addr(addr);
    closeSocket(sockfd);
    return -1;
  }

  /* Send notification message to tracker
  Format: FILENAME:SEGMENT_ID */
  char message[MAX_FILENAME_LEN + 20];
  snprintf(message, sizeof(message), "%s:%u", filename, segment_id);

  if (sendSocket(sockfd, message, strlen(message), 0) == (size_t)-1) {
    printf("Failed to send notification to tracker\n");
    removeIPV4Addr(addr);
    closeSocket(sockfd);
    return -1;
  }

  printf("Tracker notified of segment %u\n", segment_id);

  removeIPV4Addr(addr);
  closeSocket(sockfd);
  return 0;
}

/* Update the local record file to track downloaded segments
Write a record file indicating which segments have been downloaded */
int update_segment_record(FileDownloadState *state, uint32_t segment_id,
                          const char *tracker_ip, uint16_t tracker_port) {
  if (!state || !tracker_ip) {
    printf("Invalid parameters for update_segment_record\n");
    return -1;
  }

  mtx_lock(&state->lock);

  // Create record filename
  char record_filename[MAX_FILENAME_LEN + 10];
  snprintf(record_filename, sizeof(record_filename), "%s.record",
           state->filename);

  // Append segment ID to record file
  FILE *record_file = fopen(record_filename, "a");
  if (!record_file) {
    printf("Failed to open record file: %s\n", record_filename);
    mtx_unlock(&state->lock);
    return -1;
  }

  fprintf(record_file, "Segment %u downloaded at %ld\n", segment_id,
          time(NULL));
  fclose(record_file);

  printf("Updated record file: %s\n", record_filename);

  mtx_unlock(&state->lock);

  // Notify tracker of this update
  return notify_tracker(tracker_ip, tracker_port, state->filename, segment_id);
}

/* Get list of peers from tracker who have segments of a file
Populates the peer swarm with peers returned by tracker */
int get_peer_list_from_tracker(const char *tracker_ip, uint16_t tracker_port,
                               const char *filename, PeerSwarm *swarm) {
  if (!tracker_ip || !filename || !swarm) {
    printf("Invalid parameters for get_peer_list_from_tracker\n");
    return -1;
  }

  printf("Requesting peer list from tracker for file: %s\n", filename);

  int32_t sockfd = createIPV4SockStream();
  if (sockfd == -1) {
    printf("Failed to create socket for peer list request\n");
    return -1;
  }

  SocketAddress *addr = createIPV4Addr(tracker_ip, tracker_port);
  if (!addr) {
    printf("Failed to create tracker address\n");
    closeSocket(sockfd);
    return -1;
  }

  if (connectToSocket(sockfd, addr) == -1) {
    printf("Failed to connect tracker\n");
    removeIPV4Addr(addr);
    closeSocket(sockfd);
    return -1;
  }

  // Send request to tracker for peer list
  char request[MAX_FILENAME_LEN + 10];
  snprintf(request, sizeof(request), "LIST:%s", filename);

  if (sendSocket(sockfd, request, strlen(request), 0) == (size_t)-1) {
    printf("Failed to send peer list request\n");
    removeIPV4Addr(addr);
    closeSocket(sockfd);
    return -1;
  }

  /* Receive peer list from tracker
  In a real implementation, this would parse a structured response */
  char buffer[4096];
  size_t bytes_received = recvSocket(sockfd, buffer, sizeof(buffer) - 1, 0);

  if (bytes_received > 0) {
    buffer[bytes_received] = '\0';
    printf("Received peer list:\n%s\n", buffer);
    // TODO: Parse buffer and add peers to swarm
  }

  removeIPV4Addr(addr);
  closeSocket(sockfd);
  return 0;
}

// Utility Functions

// Print download progress
void print_download_progress(FileDownloadState *state) {
  if (!state || !state->segments) {
    return;
  }

  mtx_lock(&state->lock);

  uint32_t downloaded = 0;
  uint64_t bytes_downloaded = 0;

  for (uint32_t i = 0; i < state->num_segments; i++) {
    if (state->segments[i].downloaded) {
      downloaded++;
      bytes_downloaded += state->segments[i].size;
    }
  }

  double progress = (state->num_segments > 0)
                        ? (100.0 * downloaded / state->num_segments)
                        : 0.0;

  printf("File progress: %s | Segments: %u/%u (%.1f%%) | Bytes: %" PRIu64
         "/%u\n",
         state->filename, downloaded, state->num_segments, progress,
         bytes_downloaded, state->total_size);
  mtx_unlock(&state->lock);
}

// Thread Worker Functions

/*
Download thread worker function
This thread continuously:
- Selects next segment to download
- Finds best peer with newest timestamp
- Downloads segment from peer
- Updates record */
int download_thread_worker(void *arg) {
  if (!arg) {
    printf("Invalid argument for download_thread_worker\n");
    return -1;
  }

  // Cast arguments - expecting a structure with state and swarm
  typedef struct {
    FileDownloadState *state;
    PeerSwarm *swarm;
    const char *tracker_ip;
    uint16_t tracker_port;
  } DownloadThreadArgs;

  DownloadThreadArgs *args = (DownloadThreadArgs *)arg;

  printf("Download worker started\n");

  // Download loop
  while (1) {
    // Select next segment to download
    FileSegment *segment = select_next_segment(args->state);
    if (!segment) {
      printf("All segments downloaded, exiting program\n");
      break;
    }

    // Select peer with newest timestamp for this segment
    PeerInfo *peer = select_peer_for_segment(args->swarm, segment->segment_id);
    if (!peer) {
      printf("No peer available for segment %u, retrying...\n",
             segment->segment_id);
      sleep_seconds(1);
      continue;
    }

    // Download the segment
    if (download_segment(peer, segment, args->state) == 0) {
      // Update record and notify tracker
      update_segment_record(args->state, segment->segment_id, args->tracker_ip,
                            args->tracker_port);
    }

    print_download_progress(args->state);
    sleep_seconds(1); // Simulate network delay
  }

  printf("Download worker exited\n");
  free(args);
  return 0;
}

/*Tracker sync thread worker function
This thread periodically contacts the tracker to:
1. Get updated peer list
2. Report current status */
int tracker_sync_thread_worker(void *arg) {
  if (!arg) {
    printf("Invalid argument for tracker_sync_thread_worker\n");
    return -1;
  }

  typedef struct {
    const char *tracker_ip;
    uint16_t tracker_port;
    const char *filename;
    PeerSwarm *swarm;
    int *should_exit;
  } SyncThreadArgs;

  SyncThreadArgs *args = (SyncThreadArgs *)arg;

  printf("Tracker sync worker started\n");

  // Sync loop will update every 10 seconds
  while (!(*args->should_exit)) {
    get_peer_list_from_tracker(args->tracker_ip, args->tracker_port,
                               args->filename, args->swarm);
    sleep_seconds(10);
  }

  printf("Tracker sync worker exited\n");
  free(args);
  return 0;
}

// Main Program
int main() {
  printf("P2P Peer Program Started\n\n");

  // Initialize peer module
  peer_init();

  // Configuration should be read from config file in real implementation
  const char *tracker_ip = "127.0.0.1";
  uint16_t tracker_port = 3490;
  const char *filename = "largefile.bin";
  uint32_t segment_size = 65536; /* 64KB segments */

  // Initialize download state
  FileDownloadState download_state = {0};
  mtx_init(&download_state.lock, mtx_plain);
  create_file_segments(&download_state, filename, segment_size);

  // Initialize peer swarm
  PeerSwarm peer_swarm = {0};
  mtx_init(&peer_swarm.lock, mtx_plain);

  // Get initial peer list from tracker
  printf("\nRetrieving peer list from tracker...\n");
  get_peer_list_from_tracker(tracker_ip, tracker_port, filename, &peer_swarm);

  // Add sample peers for demonstration
  if (peer_swarm.num_peers == 0) {
    printf("No peers returned from tracker, using demo peers\n");
    add_peer(&peer_swarm, "192.168.1.101", 5001);
    add_peer(&peer_swarm, "192.168.1.102", 5002);
    add_peer(&peer_swarm, "192.168.1.103", 5003);
  }

  printf("\nStarting to download threads...\n");

  // Create and launch download threads
  thrd_t download_thread1;

  typedef struct {
    FileDownloadState *state;
    PeerSwarm *swarm;
    const char *tracker_ip;
    uint16_t tracker_port;
  } DownloadThreadArgs;

  // Launch download worker threads
  DownloadThreadArgs *args1 = (DownloadThreadArgs *)malloc(sizeof(*args1));
  args1->state = &download_state;
  args1->swarm = &peer_swarm;
  args1->tracker_ip = tracker_ip;
  args1->tracker_port = tracker_port;

  if (thrd_create(&download_thread1, download_thread_worker, args1) ==
      thrd_success) {
    thrd_detach(download_thread1);
  } else {
    printf("Failed to create download thread\n");
    free(args1);
  }

  printf("Download threads created\n");
  printf("# of File segments: %u, Segment size: %u bytes\n",
         download_state.num_segments, segment_size);

  printf("\nDownload in Progress:\n");

  // Let download proceed for a while
  sleep_seconds(1);

  printf("\nFinal Status:\n");
  print_download_progress(&download_state);

  // Cleanup
  printf("\nCleaning up..\n");
  free_file_segments(&download_state);
  mtx_destroy(&download_state.lock);
  mtx_destroy(&peer_swarm.lock);
  peer_cleanup();

  printf("P2P Peer Program Completed\n");

  return 0;
}

// TODO: Remove when done.
// Socket API Stub Implementations for standalone testing

void initSocketAPI(void) { printf("Socket API initialized (stub)\n"); }

void cleanupSocketAPI(void) { printf("Socket API cleaned up (stub)\n"); }

int32_t createSocketFileDescriptor(int32_t domain, int32_t type,
                                   int32_t protocol) {
  printf("Creating socket (stub) - domain: %d, type: %d, protocol: %d\n",
         domain, type, protocol);
  return 1; // Return dummy socket fd
}

SocketAddress *createIPV4Addr(const char *ip, uint16_t port) {
  printf("Creating IPv4 address (stub) - IP: %s, port: %u\n", ip, port);
  SocketAddress *addr = (SocketAddress *)malloc(sizeof(SocketAddress));
  if (addr) {
    addr->length = 16; // IPv4 address length
    addr->storage = malloc(16);
  }
  return addr;
}

void removeIPV4Addr(SocketAddress *addr) {
  if (addr) {
    if (addr->storage)
      free(addr->storage);
    free(addr);
    printf("Removed IPv4 address (stub)\n");
  }
}

int32_t connectToSocket(int32_t sockfd, const SocketAddress *address) {
  (void)address;
  printf("Connecting to socket (stub) - fd: %d\n", sockfd);
  return 0; // Success
}

size_t sendSocket(int32_t sockfd, const void *buffer, uint32_t len,
                  int32_t flags) {
  (void)flags;
  (void)buffer;
  printf("Sending data (stub) - fd: %d, len: %u\n", sockfd, len);
  return len; // Pretend we sent all bytes
}

size_t recvSocket(int32_t sockfd, const void *buffer, uint32_t len,
                  int32_t flags) {
  (void)flags;
  (void)buffer;
  printf("Receiving data (stub) - fd: %d, len: %u\n", sockfd, len);
  return 0; // No data received
}

int32_t bindSocket(int32_t sockfd, const SocketAddress *address) {
  (void)address;
  printf("Binding socket (stub) - fd: %d\n", sockfd);
  return 0; // Success
}

int32_t createIPV4SockStream() {
  printf("Creating IPV4 socket stream (stub)\n");
  return 0;
}

void closeSocket(int32_t sockfd) {
  printf("Closing socket (stub) - fd: %d\n", sockfd);
}

uint32_t netToHostLong(uint32_t netlong) {
  printf("Stubbing netToHostLong %d\n", netlong);
  return 0;
}
