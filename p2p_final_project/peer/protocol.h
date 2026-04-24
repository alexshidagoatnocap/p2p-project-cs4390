#ifndef PEER_PROTOCOL_H
#define PEER_PROTOCOL_H

#include "peer.h"

/*
 * Safe socket helpers.
 */
int peer_recv_line(int sockfd, char *buffer, size_t buffer_size);
int peer_send_all(int sockfd, const void *buffer, size_t length);
int peer_sendf(int sockfd, const char *fmt, ...);

/*
 * Simple string helpers.
 */
void peer_trim(char *str);
void peer_strip_brackets(char *str);

/*
 * Build tracker protocol messages.
 */
void build_tracker_createtracker(char *out, size_t out_size,
                                 const char *filename,
                                 long filesize,
                                 const char *description,
                                 const char *md5,
                                 const char *ip,
                                 int port);

void build_tracker_updatetracker(char *out, size_t out_size,
                                 const char *filename,
                                 long start_byte,
                                 long end_byte,
                                 const char *ip,
                                 int port);

void build_tracker_list_request(char *out, size_t out_size);
void build_tracker_get_request(char *out, size_t out_size, const char *track_filename);

/*
 * Build peer-to-peer chunk request.
 *
 * Example:
 *   <GETCHUNK hello.txt 0 1024>\n
 */
void build_peer_chunk_request(char *out, size_t out_size,
                              const char *filename,
                              long start_byte,
                              long chunk_size);

/*
 * Parse a peer-to-peer chunk request.
 * Expects:
 *   GETCHUNK filename start size
 *
 * Returns 0 on success, -1 on failure.
 */
int parse_peer_chunk_request(char *line,
                             char *filename,
                             size_t filename_size,
                             long *start_byte,
                             long *chunk_size);

/*
 * Parse one peer line from a cached .track file:
 *   ip:port:start:end:timestamp
 */
int parse_peer_track_line(const char *line, PeerTrackEntry *entry);

/*
 * Load the text content of a cached .track file into PeerTrackFile.
 * This is peer-side equivalent of tracker-side parsing.
 */
int parse_cached_trackfile(const char *path, PeerTrackFile *track);

#endif