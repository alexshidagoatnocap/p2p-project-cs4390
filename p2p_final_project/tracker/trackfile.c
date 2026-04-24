#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "trackfile.h"

/*
 * Simple helper to trim whitespace from a string.
 * Removes spaces, tabs, and newlines from both ends.
 */
static void trim_local(char *str) {
    if (!str) return;

    size_t len = strlen(str);
    while (len > 0 &&
           (str[len - 1] == '\n' || str[len - 1] == '\r' ||
            str[len - 1] == ' '  || str[len - 1] == '\t')) {
        str[len - 1] = '\0';
        len--;
    }

    size_t start = 0;
    while (str[start] == ' ' || str[start] == '\t' ||
           str[start] == '\n' || str[start] == '\r') {
        start++;
    }

    if (start > 0) {
        memmove(str, str + start, strlen(str + start) + 1);
    }
}

/*
 * Build full path for a .track file.
 *
 * Example:
 *   filename = hello.txt
 *   -> torrents/hello.txt.track
 */
void build_trackfile_path(const char *torrents_dir,
                          const char *filename,
                          char *out_path,
                          size_t out_size) {
    if (!torrents_dir || !filename || !out_path || out_size == 0) {
        return;
    }

    snprintf(out_path, out_size, "%s/%s.track", torrents_dir, filename);
}

/*
 * Check if a track file exists.
 */
bool trackfile_exists(const char *path) {
    if (!path) return false;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return false;
    }

    fclose(fp);
    return true;
}

/*
 * Initialize a trackfile from a createtracker request.
 *
 * This creates:
 *   - file metadata (name, size, md5, etc.)
 *   - first peer entry (the creator of the file)
 */
TrackerStatus trackfile_init_from_create(const TrackerRequest *request,
                                         TrackFile *track) {
    if (!request || !track) {
        return TRACKER_ERR_PARSE;
    }

    memset(track, 0, sizeof(TrackFile));

    strncpy(track->header.filename, request->filename, MAX_FILENAME_LEN - 1);
    track->header.filename[MAX_FILENAME_LEN - 1] = '\0';

    track->header.filesize = request->filesize;

    strncpy(track->header.description, request->description, MAX_DESC_LEN - 1);
    track->header.description[MAX_DESC_LEN - 1] = '\0';

    strncpy(track->header.md5, request->md5, MAX_MD5_LEN - 1);
    track->header.md5[MAX_MD5_LEN - 1] = '\0';

    strncpy(track->peers[0].ip, request->ip, MAX_IP_LEN - 1);
    track->peers[0].ip[MAX_IP_LEN - 1] = '\0';
    track->peers[0].port = request->port;
    track->peers[0].start_byte = 0;
    track->peers[0].end_byte = request->filesize;
    track->peers[0].timestamp = (long)time(NULL);

    track->peer_count = 1;

    return TRACKER_OK;
}

/*
 * Parse a peer entry line from a track file.
 *
 * Format:
 *   ip:port:start:end:timestamp
 */
TrackerStatus parse_peer_line(const char *line, PeerEntry *peer) {
    if (!line || !peer) {
        return TRACKER_ERR_PARSE;
    }

    char temp[MAX_LINE];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    trim_local(temp);

    if (strlen(temp) == 0) {
        return TRACKER_ERR_PARSE;
    }

    char ip[MAX_IP_LEN];
    int port = 0;
    long start = 0;
    long end = 0;
    long ts = 0;

    int matched = sscanf(temp, "%63[^:]:%d:%ld:%ld:%ld",
                         ip, &port, &start, &end, &ts);

    if (matched != 5) {
        return TRACKER_ERR_PARSE;
    }

    memset(peer, 0, sizeof(PeerEntry));
    strncpy(peer->ip, ip, MAX_IP_LEN - 1);
    peer->ip[MAX_IP_LEN - 1] = '\0';
    peer->port = port;
    peer->start_byte = start;
    peer->end_byte = end;
    peer->timestamp = ts;

    return TRACKER_OK;
}

/*
 * Convert a PeerEntry struct into a string line.
 *
 * Output format:
 *   ip:port:start:end:timestamp
 */
void format_peer_line(const PeerEntry *peer, char *out, size_t out_size) {
    if (!peer || !out || out_size == 0) {
        return;
    }

    snprintf(out, out_size, "%s:%d:%ld:%ld:%ld",
             peer->ip,
             peer->port,
             peer->start_byte,
             peer->end_byte,
             peer->timestamp);
}

/*
 * Load a .track file into memory.
 *
 * Reads:
 *   - file metadata
 *   - list of peers
 */
TrackerStatus load_trackfile(const char *path, TrackFile *track) {
    if (!path || !track) {
        return TRACKER_ERR_PARSE;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return TRACKER_ERR_FILE;
    }

    memset(track, 0, sizeof(TrackFile));

    char line[MAX_LINE];

    while (fgets(line, sizeof(line), fp)) {
        trim_local(line);

        if (strlen(line) == 0) {
            continue;
        }

        if (line[0] == '#') {
            continue;
        }

        if (strncmp(line, "Filename:", 9) == 0) {
            char *value = line + 9;
            trim_local(value);
            strncpy(track->header.filename, value, MAX_FILENAME_LEN - 1);
            track->header.filename[MAX_FILENAME_LEN - 1] = '\0';
        }
        else if (strncmp(line, "Filesize:", 9) == 0) {
            char *value = line + 9;
            trim_local(value);
            track->header.filesize = atol(value);
        }
        else if (strncmp(line, "Description:", 12) == 0) {
            char *value = line + 12;
            trim_local(value);
            strncpy(track->header.description, value, MAX_DESC_LEN - 1);
            track->header.description[MAX_DESC_LEN - 1] = '\0';
        }
        else if (strncmp(line, "MD5:", 4) == 0) {
            char *value = line + 4;
            trim_local(value);
            strncpy(track->header.md5, value, MAX_MD5_LEN - 1);
            track->header.md5[MAX_MD5_LEN - 1] = '\0';
        }
        else {
            if (track->peer_count < MAX_TRACK_PEERS) {
                PeerEntry peer;
                if (parse_peer_line(line, &peer) == TRACKER_OK) {
                    track->peers[track->peer_count++] = peer;
                }
            }
        }
    }

    fclose(fp);

    if (strlen(track->header.filename) == 0) {
        return TRACKER_ERR_PARSE;
    }

    return TRACKER_OK;
}

/*
 * Save a TrackFile struct to disk.
 */
TrackerStatus save_trackfile(const char *path, const TrackFile *track) {
    if (!path || !track) {
        return TRACKER_ERR_PARSE;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        return TRACKER_ERR_FILE;
    }

    fprintf(fp, "Filename: %s\n", track->header.filename);
    fprintf(fp, "Filesize: %ld\n", track->header.filesize);
    fprintf(fp, "Description: %s\n", track->header.description);
    fprintf(fp, "MD5: %s\n", track->header.md5);
    fprintf(fp, "#list of peers\n");

    for (int i = 0; i < track->peer_count; i++) {
        char peer_line[MAX_LINE];
        format_peer_line(&track->peers[i], peer_line, sizeof(peer_line));
        fprintf(fp, "%s\n", peer_line);
    }

    fclose(fp);
    return TRACKER_OK;
}

/*
 * Find a peer in the trackfile by IP and port.
 */
int find_peer_entry(const TrackFile *track, const char *ip, int port) {
    if (!track || !ip) {
        return -1;
    }

    for (int i = 0; i < track->peer_count; i++) {
        if (strcmp(track->peers[i].ip, ip) == 0 &&
            track->peers[i].port == port) {
            return i;
        }
    }

    return -1;
}

/*
 * Update existing peer or add new peer to the trackfile.
 */
TrackerStatus update_or_add_peer(TrackFile *track,
                                 const char *ip,
                                 int port,
                                 long start_byte,
                                 long end_byte,
                                 long timestamp) {
    if (!track || !ip) {
        return TRACKER_ERR_PARSE;
    }

    int idx = find_peer_entry(track, ip, port);

    if (idx >= 0) {
        track->peers[idx].start_byte = start_byte;
        track->peers[idx].end_byte = end_byte;
        track->peers[idx].timestamp = timestamp;
        return TRACKER_OK;
    }

    if (track->peer_count >= MAX_TRACK_PEERS) {
        return TRACKER_ERR_INTERNAL;
    }

    PeerEntry *peer = &track->peers[track->peer_count++];

    memset(peer, 0, sizeof(PeerEntry));
    strncpy(peer->ip, ip, MAX_IP_LEN - 1);
    peer->ip[MAX_IP_LEN - 1] = '\0';
    peer->port = port;
    peer->start_byte = start_byte;
    peer->end_byte = end_byte;
    peer->timestamp = timestamp;

    return TRACKER_OK;
}

/*
 * Remove peers that have timed out.
 */
int remove_dead_peers(TrackFile *track,
                      long current_time,
                      long timeout_seconds) {
    if (!track) {
        return 0;
    }

    int write_idx = 0;
    int removed = 0;

    for (int read_idx = 0; read_idx < track->peer_count; read_idx++) {
        long age = current_time - track->peers[read_idx].timestamp;

        if (age > timeout_seconds) {
            removed++;
            continue;
        }

        if (write_idx != read_idx) {
            track->peers[write_idx] = track->peers[read_idx];
        }
        write_idx++;
    }

    track->peer_count = write_idx;
    return removed;
}

/*
 * Print trackfile contents (for debugging).
 */
void print_trackfile(const TrackFile *track) {
    if (!track) return;

    printf("[TRACKFILE]\n");
    printf("  Filename: %s\n", track->header.filename);
    printf("  Filesize: %ld\n", track->header.filesize);
    printf("  Description: %s\n", track->header.description);
    printf("  MD5: %s\n", track->header.md5);
    printf("  Peer Count: %d\n", track->peer_count);

    for (int i = 0; i < track->peer_count; i++) {
        printf("  Peer %d: %s:%d:%ld:%ld:%ld\n",
               i + 1,
               track->peers[i].ip,
               track->peers[i].port,
               track->peers[i].start_byte,
               track->peers[i].end_byte,
               track->peers[i].timestamp);
    }
}