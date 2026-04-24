#ifndef TRACKER_CLIENT_H
#define TRACKER_CLIENT_H

#include "peer.h"

/*
 * Sends a raw tracker message and returns the full text response.
 *
 * The response buffer is filled with whatever the tracker sends before closing
 * the connection.
 *
 * Returns:
 *   0 on success
 *  -1 on failure
 */
int send_tracker_message(PeerContext *ctx,
                         const char *message,
                         char *response,
                         size_t response_size);

/*
 * Create a tracker entry for a fully shared file.
 *
 * The filename must exist in ctx->server_cfg.shared_dir.
 * The file size is read automatically from disk.
 *
 * Returns:
 *   0 on success
 *  -1 on failure
 */
int tracker_createtracker(PeerContext *ctx, const char *filename);

/*
 * Send updatetracker for a byte range that this peer currently has.
 *
 * Returns:
 *   0 on success
 *  -1 on failure
 */
int tracker_updatetracker(PeerContext *ctx,
                          const char *filename,
                          long start_byte,
                          long end_byte);

/*
 * Request the tracker list and print/store the response in out_response.
 *
 * Returns:
 *   0 on success
 *  -1 on failure
 */
int tracker_list(PeerContext *ctx,
                 char *out_response,
                 size_t out_size);

/*
 * Request a .track file from the tracker and save it to:
 *   ctx->cache_dir/<track_filename>
 *
 * Also returns the tracker text response in out_response if requested.
 *
 * Returns:
 *   0 on success
 *  -1 on failure
 */
int tracker_gettrack(PeerContext *ctx,
                     const char *track_filename,
                     char *out_response,
                     size_t out_size);

/*
 * Build:
 *   <cache_dir>/<track_filename>
 */
void build_cache_track_path(const PeerContext *ctx,
                            const char *track_filename,
                            char *out_path,
                            size_t out_size);

#endif