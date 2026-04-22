#ifndef PEER_H
#define PEER_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#define PEER_MAX_LINE            4096
#define PEER_MAX_PATH             512
#define PEER_MAX_FILENAME         256
#define PEER_MAX_DESC             256
#define PEER_MAX_MD5               33
#define PEER_MAX_IP                64
#define PEER_MAX_TRACK_PEERS      256
#define PEER_MAX_SEGMENTS        8192
#define PEER_BACKLOG              64
#define PEER_MAX_CHUNK_SIZE     1024

/*
 * Tracker config read from clientThreadConfig.cfg
 * line 1: tracker port
 * line 2: tracker IP
 * line 3: periodic update interval in seconds
 */
typedef struct {
    int tracker_port;
    char tracker_ip[PEER_MAX_IP];
    long update_interval_seconds;
} ClientConfig;

/*
 * Peer server config read from serverThreadConfig.cfg
 * line 1: listen port
 * line 2: shared folder name
 */
typedef struct {
    int listen_port;
    char shared_dir[PEER_MAX_PATH];
} ServerConfig;

/*
 * One peer entry parsed from a .track file.
 */
typedef struct {
    char ip[PEER_MAX_IP];
    int port;
    long start_byte;
    long end_byte;
    long timestamp;
} PeerTrackEntry;

/*
 * Header information from a cached .track file.
 */
typedef struct {
    char filename[PEER_MAX_FILENAME];
    long filesize;
    char description[PEER_MAX_DESC];
    char md5[PEER_MAX_MD5];
} PeerTrackHeader;

/*
 * Full cached tracker metadata on peer side.
 */
typedef struct {
    PeerTrackHeader header;
    PeerTrackEntry peers[PEER_MAX_TRACK_PEERS];
    int peer_count;
} PeerTrackFile;

/*
 * One download segment in the local state file.
 */
typedef struct {
    long start_byte;
    long end_byte;
    int complete;   /* 0 = incomplete, 1 = complete */
} SegmentState;

/*
 * In-memory download state for one file.
 */
typedef struct {
    char filename[PEER_MAX_FILENAME];
    long filesize;
    long segment_size;
    int segment_count;
    SegmentState segments[PEER_MAX_SEGMENTS];
} DownloadState;

/*
 * Shared runtime state for one peer process.
 */
typedef struct {
    ClientConfig client_cfg;
    ServerConfig server_cfg;

    char cache_dir[PEER_MAX_PATH];
    char state_dir[PEER_MAX_PATH];
    char downloads_dir[PEER_MAX_PATH];

    int running;

    pthread_mutex_t download_lock;
    pthread_mutex_t state_lock;
} PeerContext;

/*
 * Arguments for server threads.
 * - For the main server thread, client_fd is unused.
 * - For a worker thread, client_fd is the accepted socket.
 */
typedef struct {
    PeerContext *ctx;
    int client_fd;
} PeerServerArgs;

/*
 * Arguments for the periodic tracker-update thread.
 */
typedef struct {
    PeerContext *ctx;
} PeriodicUpdateArgs;

/*
 * One worker thread downloads one segment.
 */
typedef struct {
    PeerContext *ctx;
    char filename[PEER_MAX_FILENAME];
    long filesize;
    long seg_start;
    long seg_end;
    char peer_ip[PEER_MAX_IP];
    int peer_port;
} DownloadTask;

#endif