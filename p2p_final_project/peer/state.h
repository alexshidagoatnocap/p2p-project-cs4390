#ifndef PEER_STATE_H
#define PEER_STATE_H

#include "peer.h"

/*
 * Build:
 *   <state_dir>/<filename>.state
 */
void build_state_file_path(const PeerContext *ctx,
                           const char *filename,
                           char *out_path,
                           size_t out_size);

/*
 * Initialize a DownloadState from filesize and segment size.
 * Segments are [start,end) style internally, where end is exclusive.
 */
int init_download_state(DownloadState *state,
                        const char *filename,
                        long filesize,
                        long segment_size);

/*
 * Save download state to disk.
 *
 * Format:
 *   filename
 *   filesize
 *   segment_size
 *   segment_count
 *   start end complete
 *   
 */
int save_download_state(const char *path, const DownloadState *state);

/*
 * Load download state from disk.
 */
int load_download_state(const char *path, DownloadState *state);

/*
 * Mark one segment complete by exact segment start/end match.
 */
int mark_segment_complete(DownloadState *state,
                          long start_byte,
                          long end_byte);

/*
 * Returns 1 if all segments are complete, 0 otherwise.
 */
int is_download_complete(const DownloadState *state);

/*
 * Find the next incomplete segment.
 * Returns 0 on success, -1 if all complete.
 */
int get_next_incomplete_segment(const DownloadState *state,
                                long *start_byte,
                                long *end_byte);

/*
 * Remove the .state file after download completion.
 */
int delete_download_state_file(const char *path);

/*
 * Debug print helper.
 */
void print_download_state(const DownloadState *state);

#endif