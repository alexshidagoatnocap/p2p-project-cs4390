#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"

/*
 * Remove leading/trailing whitespace from a string in place.
 */
static void trim(char *s) {
    if (!s) return;

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }

    size_t start = 0;
    while (s[start] && isspace((unsigned char)s[start])) {
        start++;
    }

    if (start > 0) {
        memmove(s, s + start, strlen(s + start) + 1);
    }
}

/*
 * Expected sconfig format:
 *
 * line 1: tracker port
 * line 2: torrents directory
 * line 3: peer timeout seconds
 *
 * Example:
 * 3490
 * torrents
 * 60
 */
TrackerStatus load_tracker_config(const char *path, TrackerConfig *config) {
    if (!path || !config) {
        return TRACKER_ERR_PARSE;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("[TRACKER CONFIG] fopen");
        return TRACKER_ERR_FILE;
    }

    char line[MAX_LINE];

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return TRACKER_ERR_PARSE;
    }
    trim(line);
    config->port = atoi(line);

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return TRACKER_ERR_PARSE;
    }
    trim(line);
    strncpy(config->torrents_dir, line, sizeof(config->torrents_dir) - 1);
    config->torrents_dir[sizeof(config->torrents_dir) - 1] = '\0';

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return TRACKER_ERR_PARSE;
    }
    trim(line);
    config->peer_timeout_seconds = atol(line);

    fclose(fp);

    if (config->port <= 0 || config->port > 65535) {
        fprintf(stderr, "[TRACKER CONFIG] Invalid port: %d\n", config->port);
        return TRACKER_ERR_PARSE;
    }

    if (strlen(config->torrents_dir) == 0) {
        fprintf(stderr, "[TRACKER CONFIG] Empty torrents directory\n");
        return TRACKER_ERR_PARSE;
    }

    if (config->peer_timeout_seconds <= 0) {
        fprintf(stderr, "[TRACKER CONFIG] Invalid peer timeout: %ld\n",
                config->peer_timeout_seconds);
        return TRACKER_ERR_PARSE;
    }

    return TRACKER_OK;
}

void print_tracker_config(const TrackerConfig *config) {
    if (!config) return;

    printf("[TRACKER CONFIG]\n");
    printf("  Port: %d\n", config->port);
    printf("  Torrents Directory: %s\n", config->torrents_dir);
    printf("  Peer Timeout: %ld sec\n", config->peer_timeout_seconds);
}