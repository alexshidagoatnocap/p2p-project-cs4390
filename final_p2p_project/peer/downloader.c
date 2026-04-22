#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "downloader.h"
#include "protocol.h"
#include "state.h"
#include "tracker_client.h"

/*
 * Build path to:
 *   cache/<track_filename>
 */
static void build_cache_path(PeerContext *ctx,
                             const char *track_filename,
                             char *out_path,
                             size_t out_size) {
    snprintf(out_path, out_size, "%s/%s", ctx->cache_dir, track_filename);
}

/*
 * Build path to:
 *   downloads/<filename>
 */
static void build_download_path(PeerContext *ctx,
                                const char *filename,
                                char *out_path,
                                size_t out_size) {
    snprintf(out_path, out_size, "%s/%s", ctx->downloads_dir, filename);
}

/*
 * Connect to a remote peer.
 */
static int connect_to_peer(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

/*
 * Write a byte range into the target file at a given offset.
 */
static int write_bytes_at_offset(const char *path,
                                 long offset,
                                 const void *buffer,
                                 size_t length) {
    FILE *fp = fopen(path, "r+b");
    if (!fp) {
        fp = fopen(path, "w+b");
        if (!fp) {
            perror("[DOWNLOADER] fopen target");
            return -1;
        }
    }

    if (fseek(fp, offset, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    size_t written = fwrite(buffer, 1, length, fp);
    fclose(fp);

    return (written == length) ? 0 : -1;
}

/*
 * Select the peer with the newest timestamp that covers the full segment.
 *
 * Returns 0 on success, -1 on failure.
 */
static int choose_peer_for_segment(const PeerTrackFile *track,
                                   long seg_start,
                                   long seg_end,
                                   char *out_ip,
                                   size_t out_ip_size,
                                   int *out_port) {
    if (!track || !out_ip || !out_port) {
        return -1;
    }

    long best_ts = -1;
    int best_idx = -1;

    for (int i = 0; i < track->peer_count; i++) {
        const PeerTrackEntry *p = &track->peers[i];

        if (p->start_byte <= seg_start && p->end_byte >= seg_end) {
            if (p->timestamp > best_ts) {
                best_ts = p->timestamp;
                best_idx = i;
            }
        }
    }

    if (best_idx < 0) {
        return -1;
    }

    strncpy(out_ip, track->peers[best_idx].ip, out_ip_size - 1);
    out_ip[out_ip_size - 1] = '\0';
    *out_port = track->peers[best_idx].port;

    return 0;
}

/*
 * Download one chunk from a remote peer.
 *
 * Protocol:
 *   send: <GETCHUNK filename start size>\n
 *   recv: <CHUNK OK filename start size>\n
 *         [raw bytes]
 *
 * Returns 0 on success, -1 on failure.
 */
static int download_one_chunk(const char *peer_ip,
                              int peer_port,
                              const char *filename,
                              long start_byte,
                              long chunk_size,
                              char *out_buffer) {
    int sock = connect_to_peer(peer_ip, peer_port);
    if (sock < 0) {
        return -1;
    }

    char request[PEER_MAX_LINE];
    build_peer_chunk_request(request, sizeof(request),
                             filename, start_byte, chunk_size);

    if (peer_send_all(sock, request, strlen(request)) < 0) {
        close(sock);
        return -1;
    }

    char line[PEER_MAX_LINE];
    int n = peer_recv_line(sock, line, sizeof(line));
    if (n <= 0) {
        close(sock);
        return -1;
    }

    peer_trim(line);

    if (strcmp(line, "<GET invalid>") == 0) {
        close(sock);
        return -1;
    }

    /*
     * Expect:
     *   <CHUNK OK filename start size>
     */
    char header_filename[PEER_MAX_FILENAME];
    long header_start = 0;
    long header_size = 0;

    if (sscanf(line, "<CHUNK OK %255s %ld %ld>",
               header_filename, &header_start, &header_size) != 3) {
        close(sock);
        return -1;
    }

    if (header_start != start_byte || header_size != chunk_size) {
        close(sock);
        return -1;
    }

    long received = 0;
    while (received < chunk_size) {
        int want = (chunk_size - received > PEER_MAX_CHUNK_SIZE)
                 ? PEER_MAX_CHUNK_SIZE
                 : (int)(chunk_size - received);

        int r = recv(sock, out_buffer + received, want, 0);
        if (r <= 0) {
            close(sock);
            return -1;
        }

        received += r;
    }

    close(sock);
    return 0;
}

void *download_segment_worker(void *arg) {
    DownloadTask *task = (DownloadTask *)arg;
    PeerContext *ctx = task->ctx;

    char download_path[PEER_MAX_PATH];
    build_download_path(ctx, task->filename, download_path, sizeof(download_path));

    long current = task->seg_start;

    while (current < task->seg_end) {
        long remaining = task->seg_end - current;
        long chunk_size = (remaining > PEER_MAX_CHUNK_SIZE)
                        ? PEER_MAX_CHUNK_SIZE
                        : remaining;

        char chunk_buf[PEER_MAX_CHUNK_SIZE];

        if (download_one_chunk(task->peer_ip,
                               task->peer_port,
                               task->filename,
                               current,
                               chunk_size,
                               chunk_buf) != 0) {
            fprintf(stderr,
                    "[DOWNLOADER] Failed chunk from %s:%d for %s [%ld, %ld)\n",
                    task->peer_ip, task->peer_port,
                    task->filename, current, current + chunk_size);
            free(task);
            return NULL;
        }

        if (write_bytes_at_offset(download_path,
                                  current,
                                  chunk_buf,
                                  (size_t)chunk_size) != 0) {
            fprintf(stderr,
                    "[DOWNLOADER] Failed to write chunk for %s at offset %ld\n",
                    task->filename, current);
            free(task);
            return NULL;
        }

        current += chunk_size;
    }

    /*
     * Mark segment complete in local state file.
     */
    char state_path[PEER_MAX_PATH];
    build_state_file_path(ctx, task->filename, state_path, sizeof(state_path));

    pthread_mutex_lock(&ctx->state_lock);

    DownloadState st;
    if (load_download_state(state_path, &st) == 0) {
        mark_segment_complete(&st, task->seg_start, task->seg_end);
        save_download_state(state_path, &st);
    }

    pthread_mutex_unlock(&ctx->state_lock);

    /*
     * Notify tracker that this peer now has this segment.
     */
    tracker_updatetracker(ctx, task->filename, task->seg_start, task->seg_end);

    printf("[DOWNLOADER] Segment complete: %s [%ld, %ld) from %s:%d\n",
           task->filename, task->seg_start, task->seg_end,
           task->peer_ip, task->peer_port);

    free(task);
    return NULL;
}

int download_file_from_cache(PeerContext *ctx, const char *track_filename) {
    if (!ctx || !track_filename) {
        return -1;
    }

    char cache_path[PEER_MAX_PATH];
    build_cache_path(ctx, track_filename, cache_path, sizeof(cache_path));

    PeerTrackFile track;
    if (parse_cached_trackfile(cache_path, &track) != 0) {
        fprintf(stderr, "[DOWNLOADER] Failed to parse cached trackfile: %s\n",
                cache_path);
        return -1;
    }

    /*
     * Use a simple fixed segment size.
     * Large enough to have multiple segments, small enough for demos.
     */
    const long segment_size = 4096;

    char state_path[PEER_MAX_PATH];
    build_state_file_path(ctx, track.header.filename, state_path, sizeof(state_path));

    DownloadState state;

    /*
     * Load old state if present, otherwise initialize a new one.
     */
    if (load_download_state(state_path, &state) != 0) {
        if (init_download_state(&state,
                                track.header.filename,
                                track.header.filesize,
                                segment_size) != 0) {
            fprintf(stderr, "[DOWNLOADER] Failed to initialize state for %s\n",
                    track.header.filename);
            return -1;
        }

        if (save_download_state(state_path, &state) != 0) {
            fprintf(stderr, "[DOWNLOADER] Failed to save initial state for %s\n",
                    track.header.filename);
            return -1;
        }
    }

    /*
     * Sequentially walk through incomplete segments, but each segment is downloaded
     * by its own worker thread. For a stronger final version, you can later launch
     * several workers at once and join them as a batch.
     */
    while (!is_download_complete(&state)) {
        long seg_start = 0;
        long seg_end = 0;

        if (get_next_incomplete_segment(&state, &seg_start, &seg_end) != 0) {
            break;
        }

        char peer_ip[PEER_MAX_IP];
        int peer_port = 0;

        if (choose_peer_for_segment(&track,
                                    seg_start,
                                    seg_end,
                                    peer_ip,
                                    sizeof(peer_ip),
                                    &peer_port) != 0) {
            fprintf(stderr,
                    "[DOWNLOADER] No peer found covering segment [%ld, %ld) for %s\n",
                    seg_start, seg_end, track.header.filename);
            return -1;
        }

        DownloadTask *task = malloc(sizeof(DownloadTask));
        if (!task) {
            return -1;
        }

        memset(task, 0, sizeof(DownloadTask));
        task->ctx = ctx;
        strncpy(task->filename, track.header.filename, PEER_MAX_FILENAME - 1);
        task->filename[PEER_MAX_FILENAME - 1] = '\0';
        task->filesize = track.header.filesize;
        task->seg_start = seg_start;
        task->seg_end = seg_end;
        strncpy(task->peer_ip, peer_ip, PEER_MAX_IP - 1);
        task->peer_ip[PEER_MAX_IP - 1] = '\0';
        task->peer_port = peer_port;

        pthread_t tid;
        if (pthread_create(&tid, NULL, download_segment_worker, task) != 0) {
            free(task);
            return -1;
        }

        /*
         * For stability, join one segment worker at a time in this version.
         * This still uses threads, but avoids too much complexity during demo.
         * You can later parallelize batches if you want.
         */
        pthread_join(tid, NULL);

        /*
         * Reload state after worker completes.
         */
        if (load_download_state(state_path, &state) != 0) {
            fprintf(stderr, "[DOWNLOADER] Failed to reload state for %s\n",
                    track.header.filename);
            return -1;
        }
    }

    if (is_download_complete(&state)) {
        printf("[DOWNLOADER] Download complete: %s\n", track.header.filename);

        /*
         * Remove state file after completion.
         */
        delete_download_state_file(state_path);

        /*
         * Remove cached tracker file after completion.
         */
        unlink(cache_path);

        return 0;
    }

    return -1;
}