#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include "peer.h"

/*
 * Download a file described by a cached .track file.
 *
 * steps in function:
 *   1. parse cache/<filename>.track
 *   2. initialize or load state/<filename>.state
 *   3. choose incomplete segments sequentially
 *   4. select a peer for each segment
 *   5. request the segment in 1024-byte chunks
 *   6. write bytes into downloads/<filename>
 *   7. mark segment complete and save state
 *   8. optionally update tracker after each completed segment
 *
 * Returns 0 on success, -1 on failure.
 */
int download_file_from_cache(PeerContext *ctx, const char *track_filename);

/*
 * Worker thread for one segment download.
 */
void *download_segment_worker(void *arg);

#endif