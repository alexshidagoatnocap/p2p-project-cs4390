#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "tracker_client.h"
#include "protocol.h"

/*
 * Local helper: connect to tracker using ctx->client_cfg
 */
static int connect_to_tracker(PeerContext *ctx) {
    if (!ctx) {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[TRACKER CLIENT] socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ctx->client_cfg.tracker_port);

    if (inet_pton(AF_INET, ctx->client_cfg.tracker_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "[TRACKER CLIENT] Invalid tracker IP: %s\n",
                ctx->client_cfg.tracker_ip);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[TRACKER CLIENT] connect");
        close(sock);
        return -1;
    }

    return sock;
}

/*
 * Local helper: get file size from shared folder.
 */
static long get_shared_file_size(PeerContext *ctx, const char *filename) {
    if (!ctx || !filename) {
        return -1;
    }

    char path[PEER_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", ctx->server_cfg.shared_dir, filename);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);

    return size;
}

int send_tracker_message(PeerContext *ctx,
                         const char *message,
                         char *response,
                         size_t response_size) {
    if (!ctx || !message) {
        return -1;
    }

    int sock = connect_to_tracker(ctx);
    if (sock < 0) {
        return -1;
    }

    if (peer_send_all(sock, message, strlen(message)) < 0) {
        perror("[TRACKER CLIENT] send");
        close(sock);
        return -1;
    }

    if (response && response_size > 0) {
        size_t used = 0;
        response[0] = '\0';

        char buf[1024];
        int n;

        while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[n] = '\0';

            if (used + (size_t)n < response_size - 1) {
                memcpy(response + used, buf, (size_t)n);
                used += (size_t)n;
                response[used] = '\0';
            }
        }
    }

    close(sock);
    return 0;
}

void build_cache_track_path(const PeerContext *ctx,
                            const char *track_filename,
                            char *out_path,
                            size_t out_size) {
    if (!ctx || !track_filename || !out_path || out_size == 0) {
        return;
    }

    snprintf(out_path, out_size, "%s/%s", ctx->cache_dir, track_filename);
}

int tracker_createtracker(PeerContext *ctx, const char *filename) {
    if (!ctx || !filename) {
        return -1;
    }

    long filesize = get_shared_file_size(ctx, filename);
    if (filesize < 0) {
        fprintf(stderr, "[TRACKER CLIENT] Shared file not found: %s/%s\n",
                ctx->server_cfg.shared_dir, filename);
        return -1;
    }

    /*
     * For now we use a placeholder description and MD5.
     * We can replace these with actual values if desired.
     */
    const char *description = "demo";
    const char *md5 = "11111111111111111111111111111111";
    //const char *ip = "127.0.0.1";
    const char *ip = ctx->server_cfg.advertised_ip;

    char message[PEER_MAX_LINE];
    build_tracker_createtracker(message, sizeof(message),
                                filename,
                                filesize,
                                description,
                                md5,
                                ip,
                                ctx->server_cfg.listen_port);

    char response[PEER_MAX_LINE];
    if (send_tracker_message(ctx, message, response, sizeof(response)) < 0) {
        return -1;
    }

    printf("[PEER] Tracker response:\n%s\n", response);
    return 0;
}

int tracker_updatetracker(PeerContext *ctx,
                          const char *filename,
                          long start_byte,
                          long end_byte) {
    if (!ctx || !filename) {
        return -1;
    }

    //const char *ip = "127.0.0.1";
    const char *ip = ctx->server_cfg.advertised_ip;

    char message[PEER_MAX_LINE];
    build_tracker_updatetracker(message, sizeof(message),
                                filename,
                                start_byte,
                                end_byte,
                                ip,
                                ctx->server_cfg.listen_port);

    char response[PEER_MAX_LINE];
    if (send_tracker_message(ctx, message, response, sizeof(response)) < 0) {
        return -1;
    }

    printf("[PEER] Tracker response:\n%s\n", response);
    return 0;
}

int tracker_list(PeerContext *ctx,
                 char *out_response,
                 size_t out_size) {
    if (!ctx) {
        return -1;
    }

    char message[PEER_MAX_LINE];
    build_tracker_list_request(message, sizeof(message));

    if (send_tracker_message(ctx, message, out_response, out_size) < 0) {
        return -1;
    }

    if (out_response) {
        printf("[PEER] Tracker LIST response:\n%s\n", out_response);
    }

    return 0;
}

int tracker_gettrack(PeerContext *ctx,
                     const char *track_filename,
                     char *out_response,
                     size_t out_size) {
    if (!ctx || !track_filename) {
        return -1;
    }

    int sock = connect_to_tracker(ctx);
    if (sock < 0) {
        return -1;
    }

    char message[PEER_MAX_LINE];
    build_tracker_get_request(message, sizeof(message), track_filename);

    if (peer_send_all(sock, message, strlen(message)) < 0) {
        perror("[TRACKER CLIENT] send GET");
        close(sock);
        return -1;
    }

    char cache_path[PEER_MAX_PATH];
    build_cache_track_path(ctx, track_filename, cache_path, sizeof(cache_path));

    FILE *cache_fp = fopen(cache_path, "w");
    if (!cache_fp) {
        perror("[TRACKER CLIENT] fopen cache");
        close(sock);
        return -1;
    }

    /*
     * We store the whole tracker response text for debugging if requested,
     * but only write the actual tracker-file body to the cache file.
     */
    if (out_response && out_size > 0) {
        out_response[0] = '\0';
    }

    int seen_begin = 0;
    int seen_end = 0;

    char line[PEER_MAX_LINE];
    while (!seen_end) {
        int n = peer_recv_line(sock, line, sizeof(line));
        if (n <= 0) {
            break;
        }

        if (out_response && out_size > 0) {
            size_t cur = strlen(out_response);
            size_t add = strlen(line);
            if (cur + add < out_size - 1) {
                memcpy(out_response + cur, line, add);
                out_response[cur + add] = '\0';
            }
        }

        char temp[PEER_MAX_LINE];
        strncpy(temp, line, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';
        peer_trim(temp);

        if (strcmp(temp, "<REP GET BEGIN>") == 0) {
            seen_begin = 1;
            continue;
        }

        if (strncmp(temp, "<REP GET END ", 13) == 0) {
            seen_end = 1;
            break;
        }

        if (seen_begin) {
            fputs(line, cache_fp);
        }
    }

    fclose(cache_fp);
    close(sock);

    if (!seen_begin || !seen_end) {
        fprintf(stderr, "[TRACKER CLIENT] Incomplete GET response for %s\n",
                track_filename);
        return -1;
    }

    printf("[PEER] Cached tracker file: %s\n", cache_path);
    if (out_response) {
        printf("[PEER] Tracker GET response:\n%s\n", out_response);
    }

    return 0;
}