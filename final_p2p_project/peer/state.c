#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "state.h"

/*
 * Build the file path for a .state file.
 *
 * Example:
 *   filename = hello.txt
 *   -> state/hello.txt.state
 *
 * This file is used to store download progress.
 */
void build_state_file_path(const PeerContext *ctx,
                           const char *filename,
                           char *out_path,
                           size_t out_size) {
    if (!ctx || !filename || !out_path || out_size == 0) {
        return;
    }

    snprintf(out_path, out_size, "%s/%s.state", ctx->state_dir, filename);
}

/*
 * Initialize a download state.
 *
 * Splits the file into segments (chunks) so we can download piece by piece.
 *
 * Example:
 *   filesize = 10000
 *   segment_size = 1000
 *   -> 10 segments
 */
int init_download_state(DownloadState *state,
                        const char *filename,
                        long filesize,
                        long segment_size) {
    if (!state || !filename || filesize < 0 || segment_size <= 0) {
        return -1;
    }

    memset(state, 0, sizeof(DownloadState));

    strncpy(state->filename, filename, PEER_MAX_FILENAME - 1);
    state->filename[PEER_MAX_FILENAME - 1] = '\0';

    state->filesize = filesize;
    state->segment_size = segment_size;

    int segment_count = (int)((filesize + segment_size - 1) / segment_size);
    if (segment_count > PEER_MAX_SEGMENTS) {
        fprintf(stderr, "[STATE] Too many segments: %d\n", segment_count);
        return -1;
    }

    state->segment_count = segment_count;

    for (int i = 0; i < segment_count; i++) {
        long start = i * segment_size;
        long end = start + segment_size;
        if (end > filesize) {
            end = filesize;
        }

        state->segments[i].start_byte = start;
        state->segments[i].end_byte = end;
        state->segments[i].complete = 0;
    }

    return 0;
}

/*
 * Save download progress to a file.
 *
 * This allows us to resume downloads later.
 */
int save_download_state(const char *path, const DownloadState *state) {
    if (!path || !state) {
        return -1;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror("[STATE] fopen save");
        return -1;
    }

    fprintf(fp, "%s\n", state->filename);
    fprintf(fp, "%ld\n", state->filesize);
    fprintf(fp, "%ld\n", state->segment_size);
    fprintf(fp, "%d\n", state->segment_count);

    for (int i = 0; i < state->segment_count; i++) {
        fprintf(fp, "%ld %ld %d\n",
                state->segments[i].start_byte,
                state->segments[i].end_byte,
                state->segments[i].complete);
    }

    fclose(fp);
    return 0;
}

/*
 * Load download progress from a .state file.
 *
 * Used when restarting a peer to continue downloading.
 */
int load_download_state(const char *path, DownloadState *state) {
    if (!path || !state) {
        return -1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    memset(state, 0, sizeof(DownloadState));

    char line[PEER_MAX_LINE];

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    peer_trim(line);
    strncpy(state->filename, line, PEER_MAX_FILENAME - 1);
    state->filename[PEER_MAX_FILENAME - 1] = '\0';

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    peer_trim(line);
    state->filesize = atol(line);

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    peer_trim(line);
    state->segment_size = atol(line);

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    peer_trim(line);
    state->segment_count = atoi(line);

    if (state->segment_count < 0 || state->segment_count > PEER_MAX_SEGMENTS) {
        fclose(fp);
        return -1;
    }

    for (int i = 0; i < state->segment_count; i++) {
        if (!fgets(line, sizeof(line), fp)) {
            fclose(fp);
            return -1;
        }

        if (sscanf(line, "%ld %ld %d",
                   &state->segments[i].start_byte,
                   &state->segments[i].end_byte,
                   &state->segments[i].complete) != 3) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

/*
 * Mark a segment as completed after downloading.
 */
int mark_segment_complete(DownloadState *state,
                          long start_byte,
                          long end_byte) {
    if (!state) {
        return -1;
    }

    for (int i = 0; i < state->segment_count; i++) {
        if (state->segments[i].start_byte == start_byte &&
            state->segments[i].end_byte == end_byte) {
            state->segments[i].complete = 1;
            return 0;
        }
    }

    return -1;
}

/*
 * Check if entire file is downloaded.
 *
 * Returns 1 if complete, 0 otherwise.
 */
int is_download_complete(const DownloadState *state) {
    if (!state) {
        return 0;
    }

    for (int i = 0; i < state->segment_count; i++) {
        if (!state->segments[i].complete) {
            return 0;
        }
    }

    return 1;
}

/*
 * Find the next segment that is not downloaded yet.
 */
int get_next_incomplete_segment(const DownloadState *state,
                                long *start_byte,
                                long *end_byte) {
    if (!state || !start_byte || !end_byte) {
        return -1;
    }

    for (int i = 0; i < state->segment_count; i++) {
        if (!state->segments[i].complete) {
            *start_byte = state->segments[i].start_byte;
            *end_byte = state->segments[i].end_byte;
            return 0;
        }
    }

    return -1;
}

/*
 * Delete the .state file after download is complete.
 */
int delete_download_state_file(const char *path) {
    if (!path) {
        return -1;
    }

    return unlink(path);
}

/*
 * Print download state (for debugging).
 */
void print_download_state(const DownloadState *state) {
    if (!state) return;

    printf("[DOWNLOAD STATE]\n");
    printf("  Filename: %s\n", state->filename);
    printf("  Filesize: %ld\n", state->filesize);
    printf("  Segment Size: %ld\n", state->segment_size);
    printf("  Segment Count: %d\n", state->segment_count);

    for (int i = 0; i < state->segment_count; i++) {
        printf("  Segment %d: [%ld, %ld) complete=%d\n",
               i,
               state->segments[i].start_byte,
               state->segments[i].end_byte,
               state->segments[i].complete);
    }
}