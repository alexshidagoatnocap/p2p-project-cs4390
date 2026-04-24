#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.h"

/*
 * Read one full line from a socket.
 *
 * Reads character by character until:
 *   - newline is found
 *   - buffer is full
 *   - connection closes
 */
int peer_recv_line(int sockfd, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return -1;
    }

    size_t i = 0;
    char c;

    while (i < buffer_size - 1) {
        int n = recv(sockfd, &c, 1, 0);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            break;
        }

        buffer[i++] = c;

        if (c == '\n') {
            break;
        }
    }

    buffer[i] = '\0';
    return (int)i;
}

/*
 * Send all bytes over socket.
 *
 * send() may not send everything in one call,
 * so we loop until all data is sent.
 */
int peer_send_all(int sockfd, const void *buffer, size_t length) {
    const char *ptr = (const char *)buffer;
    size_t total_sent = 0;

    while (total_sent < length) {
        int n = send(sockfd, ptr + total_sent, length - total_sent, 0);
        if (n <= 0) {
            return -1;
        }
        total_sent += (size_t)n;
    }

    return 0;
}

/*
 * Send formatted message to socket.
 *
 * Works like printf but sends to socket.
 */
int peer_sendf(int sockfd, const char *fmt, ...) {
    char out[PEER_MAX_LINE];
    va_list args;

    va_start(args, fmt);
    vsnprintf(out, sizeof(out), fmt, args);
    va_end(args);

    return peer_send_all(sockfd, out, strlen(out));
}

/*
 * Remove whitespace from both ends of string.
 */
void peer_trim(char *str) {
    if (!str) return;

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }

    size_t start = 0;
    while (str[start] && isspace((unsigned char)str[start])) {
        start++;
    }

    if (start > 0) {
        memmove(str, str + start, strlen(str + start) + 1);
    }
}

/*
 * Remove < and > from protocol messages.
 *
 * Example:
 *   "<GET file>" → "GET file"
 */
void peer_strip_brackets(char *str) {
    if (!str) return;

    peer_trim(str);

    size_t len = strlen(str);
    if (len >= 2 && str[0] == '<' && str[len - 1] == '>') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
        peer_trim(str);
    }
}

/*
 * Build createtracker message to send to tracker.
 */
void build_tracker_createtracker(char *out, size_t out_size,
                                 const char *filename,
                                 long filesize,
                                 const char *description,
                                 const char *md5,
                                 const char *ip,
                                 int port) {
    if (!out || out_size == 0) return;

    snprintf(out, out_size,
             "<createtracker %s %ld %s %s %s %d>\n",
             filename, filesize, description, md5, ip, port);
}

/*
 * Build updatetracker message.
 */
void build_tracker_updatetracker(char *out, size_t out_size,
                                 const char *filename,
                                 long start_byte,
                                 long end_byte,
                                 const char *ip,
                                 int port) {
    if (!out || out_size == 0) return;

    snprintf(out, out_size,
             "<updatetracker %s %ld %ld %s %d>\n",
             filename, start_byte, end_byte, ip, port);
}

/*
 * Build REQ LIST message.
 */
void build_tracker_list_request(char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    snprintf(out, out_size, "<REQ LIST>\n");
}

/*
 * Build GET request for tracker file.
 */
void build_tracker_get_request(char *out, size_t out_size, const char *track_filename) {
    if (!out || out_size == 0) return;
    snprintf(out, out_size, "<GET %s>\n", track_filename);
}

/*
 * Build request for file chunk from another peer.
 *
 * Format:
 *   <GETCHUNK filename start size>
 */
void build_peer_chunk_request(char *out, size_t out_size,
                              const char *filename,
                              long start_byte,
                              long chunk_size) {
    if (!out || out_size == 0) return;

    snprintf(out, out_size,
             "<GETCHUNK %s %ld %ld>\n",
             filename, start_byte, chunk_size);
}

/*
 * Parse GETCHUNK request from another peer.
 */
int parse_peer_chunk_request(char *line,
                             char *filename,
                             size_t filename_size,
                             long *start_byte,
                             long *chunk_size) {
    if (!line || !filename || !start_byte || !chunk_size) {
        return -1;
    }

    peer_trim(line);
    peer_strip_brackets(line);

    char cmd[64];
    memset(cmd, 0, sizeof(cmd));

    if (sscanf(line, "%63s %255s %ld %ld",
               cmd, filename, start_byte, chunk_size) != 4) {
        return -1;
    }

    filename[filename_size - 1] = '\0';

    if (strcmp(cmd, "GETCHUNK") != 0) {
        return -1;
    }

    return 0;
}

/*
 * Parse a single peer entry from a track file.
 *
 * Format:
 *   ip:port:start:end:timestamp
 */
int parse_peer_track_line(const char *line, PeerTrackEntry *entry) {
    if (!line || !entry) {
        return -1;
    }

    char temp[PEER_MAX_LINE];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    peer_trim(temp);

    if (strlen(temp) == 0) {
        return -1;
    }

    char ip[PEER_MAX_IP];
    int port = 0;
    long start = 0;
    long end = 0;
    long ts = 0;

    int matched = sscanf(temp, "%63[^:]:%d:%ld:%ld:%ld",
                         ip, &port, &start, &end, &ts);

    if (matched != 5) {
        return -1;
    }

    memset(entry, 0, sizeof(PeerTrackEntry));
    strncpy(entry->ip, ip, PEER_MAX_IP - 1);
    entry->ip[PEER_MAX_IP - 1] = '\0';
    entry->port = port;
    entry->start_byte = start;
    entry->end_byte = end;
    entry->timestamp = ts;

    return 0;
}

/*
 * Parse entire cached .track file.
 *
 * Reads:
 *   - metadata (filename, size, etc.)
 *   - list of peers
 */
int parse_cached_trackfile(const char *path, PeerTrackFile *track) {
    if (!path || !track) {
        return -1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    memset(track, 0, sizeof(PeerTrackFile));

    char line[PEER_MAX_LINE];

    while (fgets(line, sizeof(line), fp)) {
        peer_trim(line);

        if (strlen(line) == 0) {
            continue;
        }

        if (line[0] == '#') {
            continue;
        }

        if (strncmp(line, "Filename:", 9) == 0) {
            char *value = line + 9;
            peer_trim(value);
            strncpy(track->header.filename, value, PEER_MAX_FILENAME - 1);
            track->header.filename[PEER_MAX_FILENAME - 1] = '\0';
        }
        else if (strncmp(line, "Filesize:", 9) == 0) {
            char *value = line + 9;
            peer_trim(value);
            track->header.filesize = atol(value);
        }
        else if (strncmp(line, "Description:", 12) == 0) {
            char *value = line + 12;
            peer_trim(value);
            strncpy(track->header.description, value, PEER_MAX_DESC - 1);
            track->header.description[PEER_MAX_DESC - 1] = '\0';
        }
        else if (strncmp(line, "MD5:", 4) == 0) {
            char *value = line + 4;
            peer_trim(value);
            strncpy(track->header.md5, value, PEER_MAX_MD5 - 1);
            track->header.md5[PEER_MAX_MD5 - 1] = '\0';
        }
        else {
            if (track->peer_count < PEER_MAX_TRACK_PEERS) {
                PeerTrackEntry entry;
                if (parse_peer_track_line(line, &entry) == 0) {
                    track->peers[track->peer_count++] = entry;
                }
            }
        }
    }

    fclose(fp);

    if (strlen(track->header.filename) == 0) {
        return -1;
    }

    return 0;
}