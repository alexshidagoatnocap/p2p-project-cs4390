#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.h"

/*
 * Read a single line from a socket.
 *
 * We read one character at a time until:
 *   - we hit '\n'
 *   - buffer is full
 *   - connection closes
 *
 * Returns number of bytes read, or -1 on error.
 */
int tracker_recv_line(int sockfd, char *buffer, size_t buffer_size) {
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
 * Send all bytes over the socket.
 *
 * send() might not send everything in one call,
 * so we loop until all data is sent.
 */
int tracker_send_all(int sockfd, const void *buffer, size_t length) {
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
 * Helper function to send formatted text (like printf but to socket).
 *
 * Example:
 *   tracker_sendf(sockfd, "<REQ LIST>\n");
 */
int tracker_sendf(int sockfd, const char *fmt, ...) {
    char out[MAX_LINE];
    va_list args;

    va_start(args, fmt);
    vsnprintf(out, sizeof(out), fmt, args);
    va_end(args);

    return tracker_send_all(sockfd, out, strlen(out));
}

/*
 * Remove leading and trailing whitespace from a string.
 */
void tracker_trim(char *str) {
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
 *   "<REQ LIST>" -> "REQ LIST"
 */
void tracker_strip_brackets(char *str) {
    if (!str) return;

    tracker_trim(str);

    size_t len = strlen(str);
    if (len >= 2 && str[0] == '<' && str[len - 1] == '>') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
        tracker_trim(str);
    }
}

/*
 * Parse a request coming from a peer.
 *
 * This converts a raw string into a structured request.
 *
 * Supported commands:
 *   createtracker
 *   updatetracker
 *   REQ LIST
 *   GET
 */
TrackerStatus parse_tracker_request(char *line, TrackerRequest *request) {
    if (!line || !request) {
        return TRACKER_ERR_PARSE;
    }

    memset(request, 0, sizeof(TrackerRequest));
    request->type = TRACKER_CMD_UNKNOWN;

    tracker_trim(line);
    tracker_strip_brackets(line);

    if (strlen(line) == 0) {
        return TRACKER_ERR_PARSE;
    }

    /*
     * Split input into tokens (words)
     * Example:
     *   "REQ LIST" -> ["REQ", "LIST"]
     */

    char *tokens[16];
    int count = 0;

    char *saveptr = NULL;
    char *tok = strtok_r(line, " \t\r\n", &saveptr);
    while (tok && count < 16) {
        tokens[count++] = tok;
        tok = strtok_r(NULL, " \t\r\n", &saveptr);
    }

    if (count == 0) {
        return TRACKER_ERR_PARSE;
    }

     /*
     * Handle: createtracker
     */
    if (strcmp(tokens[0], "createtracker") == 0) {
        if (count < 7) {
            return TRACKER_ERR_PARSE;
        }

        request->type = TRACKER_CMD_CREATE;

        strncpy(request->filename, tokens[1], MAX_FILENAME_LEN - 1);
        request->filename[MAX_FILENAME_LEN - 1] = '\0';

        request->filesize = atol(tokens[2]);

        strncpy(request->description, tokens[3], MAX_DESC_LEN - 1);
        request->description[MAX_DESC_LEN - 1] = '\0';

        strncpy(request->md5, tokens[4], MAX_MD5_LEN - 1);
        request->md5[MAX_MD5_LEN - 1] = '\0';

        strncpy(request->ip, tokens[5], MAX_IP_LEN - 1);
        request->ip[MAX_IP_LEN - 1] = '\0';

        request->port = atoi(tokens[6]);

        return TRACKER_OK;
    }
    /*
     * Handle: updatetracker
     */

    if (strcmp(tokens[0], "updatetracker") == 0) {
        if (count < 6) {
            return TRACKER_ERR_PARSE;
        }

        request->type = TRACKER_CMD_UPDATE;

        strncpy(request->filename, tokens[1], MAX_FILENAME_LEN - 1);
        request->filename[MAX_FILENAME_LEN - 1] = '\0';

        request->start_byte = atol(tokens[2]);
        request->end_byte = atol(tokens[3]);

        strncpy(request->ip, tokens[4], MAX_IP_LEN - 1);
        request->ip[MAX_IP_LEN - 1] = '\0';

        request->port = atoi(tokens[5]);

        return TRACKER_OK;
    }

    /*
     * Handle: REQ LIST
     */
    if (count >= 2 && strcmp(tokens[0], "REQ") == 0 && strcmp(tokens[1], "LIST") == 0) {
        request->type = TRACKER_CMD_LIST;
        return TRACKER_OK;
    }

    /*
     * Handle: GET filename.track
     */
    if (strcmp(tokens[0], "GET") == 0) {
        if (count < 2) {
            return TRACKER_ERR_PARSE;
        }

        request->type = TRACKER_CMD_GET;
        strncpy(request->filename, tokens[1], MAX_FILENAME_LEN - 1);
        request->filename[MAX_FILENAME_LEN - 1] = '\0';

        return TRACKER_OK;
    }

    return TRACKER_ERR_PARSE;
}

/*
 * Build response for createtracker command.
 */
void build_create_response(char *out, size_t out_size, TrackerStatus status) {
    if (!out || out_size == 0) return;

    switch (status) {
        case TRACKER_OK:
            snprintf(out, out_size, "<createtracker succ>\n");
            break;
        case TRACKER_ERR_EXISTS:
            snprintf(out, out_size, "<createtracker ferr>\n");
            break;
        default:
            snprintf(out, out_size, "<createtracker fail>\n");
            break;
    }
}

/*
 * Build response for updatetracker command.
 */
void build_update_response(char *out, size_t out_size,
                           const char *filename, TrackerStatus status) {
    if (!out || out_size == 0) return;

    const char *name = filename ? filename : "?";

    switch (status) {
        case TRACKER_OK:
            snprintf(out, out_size, "<updatetracker %s succ>\n", name);
            break;
        case TRACKER_ERR_NOT_FOUND:
            snprintf(out, out_size, "<updatetracker %s ferr>\n", name);
            break;
        default:
            snprintf(out, out_size, "<updatetracker %s fail>\n", name);
            break;
    }
}

/*
 * Generic error response.
 */
void build_error_response(char *out, size_t out_size, const char *message) {
    if (!out || out_size == 0) return;

    const char *msg = message ? message : "unknown_error";
    snprintf(out, out_size, "<ERROR %s>\n", msg);
}