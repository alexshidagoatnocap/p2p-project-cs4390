#ifndef TRACKFILE_H
#define TRACKFILE_H

#include "tracker.h"

/*
 * Build:
 *   <torrents_dir>/<filename>.track
 */
void build_trackfile_path(const char *torrents_dir,
                          const char *filename,
                          char *out_path,
                          size_t out_size);

/*
 * Check whether a .track file exists on disk.
 */
bool trackfile_exists(const char *path);

/*
 * Initialize an in-memory TrackFile from a createtracker request.
 * Adds the first peer entry:
 *   ip:port:0:filesize:current_time
 */
TrackerStatus trackfile_init_from_create(const TrackerRequest *request,
                                         TrackFile *track);

/*
 * Load a .track file from disk into memory.
 */
TrackerStatus load_trackfile(const char *path, TrackFile *track);

/*
 * Save an in-memory TrackFile back to disk.
 */
TrackerStatus save_trackfile(const char *path, const TrackFile *track);

/*
 * Find peer by ip + port.
 * Returns index if found, otherwise -1.
 */
int find_peer_entry(const TrackFile *track, const char *ip, int port);

/*
 * Update existing peer entry or append a new one.
 */
TrackerStatus update_or_add_peer(TrackFile *track,
                                 const char *ip,
                                 int port,
                                 long start_byte,
                                 long end_byte,
                                 long timestamp);

/*
 * Remove peers that have timed out.
 * Returns number of removed peers.
 */
int remove_dead_peers(TrackFile *track,
                      long current_time,
                      long timeout_seconds);

/*
 * Parse a peer line:
 *   ip:port:start:end:timestamp
 */
TrackerStatus parse_peer_line(const char *line, PeerEntry *peer);

/*
 * Format a peer line:
 *   ip:port:start:end:timestamp
 */
void format_peer_line(const PeerEntry *peer,
                      char *out,
                      size_t out_size);

/*
 * Debug helper.
 */
void print_trackfile(const TrackFile *track);

#endif