#ifndef TRACKER_PROTOCOL_H
#define TRACKER_PROTOCOL_H

#include "tracker.h"

/*
 * Read one line from a socket.
 * Stops at:
 *   - '\n'
 *   - closed connection
 *   - full buffer
 *
 * Returns:
 *   >0 number of bytes read
 *    0 connection closed cleanly
 *   -1 on socket error
 */
int tracker_recv_line(int sockfd, char *buffer, size_t buffer_size);

/*
 * Send exactly length bytes.
 * Returns 0 on success, -1 on failure.
 */
int tracker_send_all(int sockfd, const void *buffer, size_t length);

/*
 * Send a formatted response string.
 * Returns 0 on success, -1 on failure.
 */
int tracker_sendf(int sockfd, const char *fmt, ...);

/*
 * Remove leading/trailing whitespace in place.
 */
void tracker_trim(char *str);

/*
 * Remove outer angle brackets if present.
 * Example:
 *   "<REQ LIST>" -> "REQ LIST"
 */
void tracker_strip_brackets(char *str);

/*
 * Parse one incoming request line into TrackerRequest.
 *
 * Supported forms:
 *   createtracker filename filesize description md5 ip port
 *   updatetracker filename start end ip port
 *   REQ LIST
 *   GET filename.track
 */
TrackerStatus parse_tracker_request(char *line, TrackerRequest *request);

/*
 * Build standard response messages.
 */
void build_create_response(char *out, size_t out_size, TrackerStatus status);
void build_update_response(char *out, size_t out_size,
                           const char *filename, TrackerStatus status);
void build_error_response(char *out, size_t out_size, const char *message);

#endif