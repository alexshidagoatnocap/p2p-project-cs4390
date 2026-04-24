#ifndef TRACKER_H
#define TRACKER_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_LINE             4096
#define MAX_PATH_LEN          512
#define MAX_FILENAME_LEN      256
#define MAX_DESC_LEN          256
#define MAX_MD5_LEN            33
#define MAX_IP_LEN             64
#define MAX_TRACK_PEERS       256
#define TRACKER_BACKLOG        64

typedef enum {
    TRACKER_CMD_UNKNOWN = 0,
    TRACKER_CMD_CREATE,
    TRACKER_CMD_UPDATE,
    TRACKER_CMD_LIST,
    TRACKER_CMD_GET
} TrackerCommandType;

typedef enum {
    TRACKER_OK = 0,
    TRACKER_ERR_FILE,
    TRACKER_ERR_PARSE,
    TRACKER_ERR_EXISTS,
    TRACKER_ERR_NOT_FOUND,
    TRACKER_ERR_INTERNAL
} TrackerStatus;

typedef struct {
    char ip[MAX_IP_LEN];
    int port;
    long start_byte;
    long end_byte;
    long timestamp;
} PeerEntry;

typedef struct {
    char filename[MAX_FILENAME_LEN];
    long filesize;
    char description[MAX_DESC_LEN];
    char md5[MAX_MD5_LEN];
} TrackHeader;

typedef struct {
    TrackHeader header;
    PeerEntry peers[MAX_TRACK_PEERS];
    int peer_count;
} TrackFile;

typedef struct {
    int port;
    char torrents_dir[MAX_PATH_LEN];
    long peer_timeout_seconds;
} TrackerConfig;

typedef struct {
    TrackerCommandType type;

    char filename[MAX_FILENAME_LEN];
    long filesize;
    char description[MAX_DESC_LEN];
    char md5[MAX_MD5_LEN];
    char ip[MAX_IP_LEN];
    int port;
    long start_byte;
    long end_byte;
} TrackerRequest;

typedef struct {
    int client_fd;
    TrackerConfig config;
    pthread_mutex_t *trackfile_mutex;
} TrackerWorkerArgs;

#endif