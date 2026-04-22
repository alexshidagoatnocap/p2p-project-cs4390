#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

#include "peer.h"
#include "config.h"
#include "protocol.h"
#include "tracker_client.h"
#include "peer_server.h"
#include "downloader.h"
#include "state.h"

/*
 * -------------------------------------------------------------------------
 * peer.c
 *
 * Main peer program for the final P2P system.
 *
 * What it does:
 *   1. Load peer configuration
 *   2. Create required runtime directories
 *   3. Start the peer server thread
 *   4. Start the periodic tracker update thread
 *   5. Resume incomplete downloads from state/
 *   6. Provide a manual command-line interface for testing
 *
 
 * -------------------------------------------------------------------------
 */


/* =========================================================================
 * Small local utility helpers
 * ========================================================================= */

/*
 * Ensure a directory exists.
 *
 * If the directory is missing, create it.
 * This is used for:
 *   - shared/
 *   - cache/
 *   - state/
 *   - downloads/
 */
static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        mkdir(path, 0777);
    }
}

/*
 * Build local path:
 *   cache/<filename>.track
 *
 * Example:
 *   hello.txt.track -> cache/hello.txt.track
 */
static void build_cache_track_path_local(PeerContext *ctx,
                                         const char *track_filename,
                                         char *out_path,
                                         size_t out_size) {
    snprintf(out_path, out_size, "%s/%s", ctx->cache_dir, track_filename);
}

/*
 * Build local path:
 *   shared/<filename>
 *
 * Example:
 *   hello.txt -> shared/hello.txt
 */
static void build_shared_file_path(PeerContext *ctx,
                                   const char *filename,
                                   char *out_path,
                                   size_t out_size) {
    snprintf(out_path, out_size, "%s/%s", ctx->server_cfg.shared_dir, filename);
}

/*
 * Return file size in bytes.
 *
 * Returns:
 *   >= 0 file size
 *   -1   on failure
 */
static long get_file_size_local(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    return size;
}

/*
 * Returns:
 *   1 if file exists
 *   0 otherwise
 */
static int file_exists_local(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}


/* =========================================================================
 * Resume support
 * ========================================================================= */

/*
 * Scan the state/ directory for existing .state files.
 *
 * For each state file:
 *   - convert filename.state -> filename.track
 *   - refresh the tracker metadata by calling tracker_gettrack()
 *   - resume the incomplete download
 *
 * This supports the project requirement that restarting a peer does not force
 * incomplete downloads to restart from zero.
 */
static void resume_incomplete_downloads(PeerContext *ctx) {
    DIR *dir = opendir(ctx->state_dir);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);

        /* Need at least something like "a.state" */
        if (len < 6) {
            continue;
        }

        /* Only care about *.state files */
        if (strcmp(entry->d_name + len - 6, ".state") != 0) {
            continue;
        }

        /*
         * Convert:
         *   hello.txt.state -> hello.txt
         */
        char filename[PEER_MAX_FILENAME];
        memset(filename, 0, sizeof(filename));
        strncpy(filename, entry->d_name, len - 6);
        filename[len - 6] = '\0';

        /*
         * Then build:
         *   hello.txt -> hello.txt.track
         */
        char track_filename[PEER_MAX_FILENAME];
        snprintf(track_filename, sizeof(track_filename), "%s.track", filename);

        /*
         * Refresh tracker metadata before resuming.
         * This makes sure the peer uses the latest view of who has the file.
         */
        tracker_gettrack(ctx, track_filename, NULL, 0);

        printf("[PEER] Resuming incomplete download: %s\n", filename);
        download_file_from_cache(ctx, track_filename);
    }

    closedir(dir);
}


/* =========================================================================
 * Periodic tracker update thread
 * ========================================================================= */

/*
 * Every update_interval_seconds:
 *   - scan the peer's shared directory
 *   - for each file, send updatetracker(filename, 0, filesize)
 *
 * This advertises to the tracker that this peer currently has the full file.
 */
static void *periodic_update_main(void *arg) {
    PeriodicUpdateArgs *args = (PeriodicUpdateArgs *)arg;
    PeerContext *ctx = args->ctx;

    while (ctx->running) {
        sleep((unsigned int)ctx->client_cfg.update_interval_seconds);

        if (!ctx->running) {
            break;
        }

        DIR *dir = opendir(ctx->server_cfg.shared_dir);
        if (!dir) {
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            /* Skip "." and ".." */
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            /* Skip hidden files */
            if (entry->d_name[0] == '.') {
                continue;
            }

            char shared_path[PEER_MAX_PATH];
            build_shared_file_path(ctx, entry->d_name,
                                   shared_path, sizeof(shared_path));

            long filesize = get_file_size_local(shared_path);
            if (filesize < 0) {
                continue;
            }

            /*
             * Advertise the full byte range [0, filesize].
             */
            tracker_updatetracker(ctx, entry->d_name, 0, filesize);
        }

        closedir(dir);
    }

    return NULL;
}


/* =========================================================================
 * Friendly command handlers
 * ========================================================================= */

/*
 * Friendly form:
 *   createtracker <filename>
 *
 * This uses helper logic in tracker_client.c to:
 *   - inspect the file in shared/
 *   - determine file size automatically
 *   - fill placeholder description/md5/ip/port values
 *   - send the full tracker protocol message
 */
static void handle_cmd_createtracker(PeerContext *ctx, const char *filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Usage: createtracker <filename>\n");
        return;
    }

    tracker_createtracker(ctx, filename);
}

/*
 * Friendly form:
 *   updatetracker <filename> <start> <end>
 *
 * This uses the peer's own configured IP/port internally.
 */
static void handle_cmd_updatetracker(PeerContext *ctx,
                                     const char *filename,
                                     long start_byte,
                                     long end_byte) {
    if (!filename || strlen(filename) == 0) {
        printf("Usage: updatetracker <filename> <start> <end>\n");
        return;
    }

    tracker_updatetracker(ctx, filename, start_byte, end_byte);
}

/*
 * Friendly form:
 *   list
 *
 * Internally this maps to tracker protocol:
 *   REQ LIST
 */
static void handle_cmd_list(PeerContext *ctx) {
    char response[PEER_MAX_LINE * 4];
    memset(response, 0, sizeof(response));

    if (tracker_list(ctx, response, sizeof(response)) != 0) {
        printf("[PEER] LIST failed.\n");
    }
}

/*
 * Friendly form:
 *   gettrack <filename.track>
 *
 * Fetches the tracker file and caches it in cache/.
 */
static void handle_cmd_gettrack(PeerContext *ctx, const char *track_filename) {
    if (!track_filename || strlen(track_filename) == 0) {
        printf("Usage: gettrack <filename.track>\n");
        return;
    }

    char response[PEER_MAX_LINE * 4];
    memset(response, 0, sizeof(response));

    if (tracker_gettrack(ctx, track_filename, response, sizeof(response)) != 0) {
        printf("[PEER] GET TRACK failed.\n");
    }
}

/*
 * Friendly form:
 *   download <filename>
 *
 * If cache/<filename>.track is missing, fetch it first.
 * Then use downloader.c to fetch the file from peers.
 */
static void handle_cmd_download(PeerContext *ctx, const char *filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Usage: download <filename>\n");
        return;
    }

    char track_filename[PEER_MAX_FILENAME];
    snprintf(track_filename, sizeof(track_filename), "%s.track", filename);

    char cache_path[PEER_MAX_PATH];
    build_cache_track_path_local(ctx, track_filename, cache_path, sizeof(cache_path));

    if (!file_exists_local(cache_path)) {
        printf("[PEER] Cached track file not found. Fetching from tracker first...\n");
        if (tracker_gettrack(ctx, track_filename, NULL, 0) != 0) {
            printf("[PEER] Could not fetch track file from tracker.\n");
            return;
        }
    }

    if (download_file_from_cache(ctx, track_filename) != 0) {
        printf("[PEER] Download failed for %s\n", filename);
        return;
    }

    /*
     * Once fully downloaded, the file stays in downloads/.
     * The peer server can already serve from downloads/ as well.
     */
    printf("[PEER] Download successful for %s\n", filename);
}


/* =========================================================================
 * Raw protocol command handlers
 * ========================================================================= */

/*
 * Raw manual form:
 *   createtracker filename filesize description md5 ip port
 *
 * Unlike the friendly version, this uses EXACTLY the values typed by the user.
 */
static void handle_cmd_createtracker_full(PeerContext *ctx,
                                          const char *filename,
                                          long filesize,
                                          const char *description,
                                          const char *md5,
                                          const char *ip,
                                          int port) {
    if (!ctx || !filename || !description || !md5 || !ip) {
        printf("Usage: createtracker <filename> <filesize> <description> <md5> <ip> <port>\n");
        return;
    }

    char message[PEER_MAX_LINE];
    build_tracker_createtracker(message, sizeof(message),
                                filename, filesize, description, md5, ip, port);

    char response[PEER_MAX_LINE];
    if (send_tracker_message(ctx, message, response, sizeof(response)) != 0) {
        printf("[PEER] createtracker failed.\n");
        return;
    }

    printf("[PEER] Tracker response:\n%s\n", response);
}

/*
 * Raw manual form:
 *   updatetracker filename start end ip port
 *
 * Uses the exact values typed by the user.
 */
static void handle_cmd_updatetracker_full(PeerContext *ctx,
                                          const char *filename,
                                          long start_byte,
                                          long end_byte,
                                          const char *ip,
                                          int port) {
    if (!ctx || !filename || !ip) {
        printf("Usage: updatetracker <filename> <start> <end> <ip> <port>\n");
        return;
    }

    char message[PEER_MAX_LINE];
    build_tracker_updatetracker(message, sizeof(message),
                                filename, start_byte, end_byte, ip, port);

    char response[PEER_MAX_LINE];
    if (send_tracker_message(ctx, message, response, sizeof(response)) != 0) {
        printf("[PEER] updatetracker failed.\n");
        return;
    }

    printf("[PEER] Tracker response:\n%s\n", response);
}

/*
 * Raw manual form:
 *   REQ LIST
 */
static void handle_cmd_req_list(PeerContext *ctx) {
    char response[PEER_MAX_LINE * 4];
    memset(response, 0, sizeof(response));

    char message[PEER_MAX_LINE];
    build_tracker_list_request(message, sizeof(message));

    if (send_tracker_message(ctx, message, response, sizeof(response)) != 0) {
        printf("[PEER] REQ LIST failed.\n");
        return;
    }

    printf("[PEER] Tracker LIST response:\n%s\n", response);
}

/*
 * Raw manual form:
 *   GET <filename.track>
 */
static void handle_cmd_get_raw(PeerContext *ctx, const char *track_filename) {
    if (!track_filename || strlen(track_filename) == 0) {
        printf("Usage: GET <filename.track>\n");
        return;
    }

    char response[PEER_MAX_LINE * 4];
    memset(response, 0, sizeof(response));

    if (tracker_gettrack(ctx, track_filename, response, sizeof(response)) != 0) {
        printf("[PEER] GET failed.\n");
        return;
    }
}


/* =========================================================================
 * Help / CLI
 * ========================================================================= */

/*
 * Print all supported commands.
 *
 * We explicitly show both:
 *   - friendly commands
 *   - raw protocol commands
 */
static void print_help(void) {
    printf("Commands:\n");
    printf("  Friendly commands:\n");
    printf("    createtracker <filename>\n");
    printf("    updatetracker <filename> <start> <end>\n");
    printf("    list\n");
    printf("    gettrack <filename.track>\n");
    printf("    download <filename>\n");
    printf("\n");
    printf("  Raw protocol commands:\n");
    printf("    REQ LIST\n");
    printf("    GET <filename.track>\n");
    printf("    createtracker <filename> <filesize> <description> <md5> <ip> <port>\n");
    printf("    updatetracker <filename> <start> <end> <ip> <port>\n");
    printf("\n");
    printf("  Other:\n");
    printf("    help\n");
    printf("    exit\n");
}

/*
 * Main manual command loop.
 *
 * This is intentionally backward-compatible:
 *   - current scripts keep working
 *   - new raw protocol manual commands also work
 */
static void command_loop(PeerContext *ctx) {
    char line[PEER_MAX_LINE];

    while (ctx->running) {
        printf("peer> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        peer_trim(line);

        if (strlen(line) == 0) {
            continue;
        }

        /*
         * We first read the first word into cmd,
         * then decide which parsing path to use.
         */
        char cmd[64];
        memset(cmd, 0, sizeof(cmd));

        if (sscanf(line, "%63s", cmd) != 1) {
            continue;
        }

        /* -------------------------------------------------------------
         * Friendly command: list
         * ------------------------------------------------------------- */
        if (strcmp(cmd, "list") == 0) {
            handle_cmd_list(ctx);
        }

        /* -------------------------------------------------------------
         * Raw protocol: REQ LIST
         * ------------------------------------------------------------- */
        else if (strcmp(cmd, "REQ") == 0) {
            char second[64];
            memset(second, 0, sizeof(second));

            if (sscanf(line, "%63s %63s", cmd, second) == 2 &&
                strcmp(second, "LIST") == 0) {
                handle_cmd_req_list(ctx);
            } else {
                printf("Usage: REQ LIST\n");
            }
        }

        /* -------------------------------------------------------------
         * Friendly command: gettrack file.track
         * ------------------------------------------------------------- */
        else if (strcmp(cmd, "gettrack") == 0) {
            char track_filename[PEER_MAX_FILENAME];
            memset(track_filename, 0, sizeof(track_filename));

            if (sscanf(line, "%63s %255s", cmd, track_filename) != 2) {
                printf("Usage: gettrack <filename.track>\n");
                continue;
            }

            handle_cmd_gettrack(ctx, track_filename);
        }

        /* -------------------------------------------------------------
         * Raw protocol: GET file.track
         * ------------------------------------------------------------- */
        else if (strcmp(cmd, "GET") == 0) {
            char track_filename[PEER_MAX_FILENAME];
            memset(track_filename, 0, sizeof(track_filename));

            if (sscanf(line, "%63s %255s", cmd, track_filename) != 2) {
                printf("Usage: GET <filename.track>\n");
                continue;
            }

            handle_cmd_get_raw(ctx, track_filename);
        }

        /* -------------------------------------------------------------
         * Friendly + raw createtracker support
         *
         * Friendly:
         *   createtracker hello.txt
         *
         * Raw:
         *   createtracker hello.txt 17 demo md5 127.0.0.1 5001
         * ------------------------------------------------------------- */
        else if (strcmp(cmd, "createtracker") == 0) {
            char filename[PEER_MAX_FILENAME];
            char description[PEER_MAX_DESC];
            char md5[PEER_MAX_MD5];
            char ip[PEER_MAX_IP];
            long filesize = 0;
            int port = 0;

            memset(filename, 0, sizeof(filename));
            memset(description, 0, sizeof(description));
            memset(md5, 0, sizeof(md5));
            memset(ip, 0, sizeof(ip));

            /*
             * Try raw full format first.
             * If all 7 items are present, use exact values provided.
             */
            int matched_full = sscanf(line,
                                      "%63s %255s %ld %255s %32s %63s %d",
                                      cmd, filename, &filesize,
                                      description, md5, ip, &port);

            if (matched_full == 7) {
                handle_cmd_createtracker_full(ctx,
                                              filename,
                                              filesize,
                                              description,
                                              md5,
                                              ip,
                                              port);
                continue;
            }

            /*
             * Otherwise try the friendly short format.
             */
            if (sscanf(line, "%63s %255s", cmd, filename) == 2) {
                handle_cmd_createtracker(ctx, filename);
                continue;
            }

            printf("Usage:\n");
            printf("  createtracker <filename>\n");
            printf("  createtracker <filename> <filesize> <description> <md5> <ip> <port>\n");
        }

        /* -------------------------------------------------------------
         * Friendly + raw updatetracker support
         *
         * Friendly:
         *   updatetracker hello.txt 0 17
         *
         * Raw:
         *   updatetracker hello.txt 0 17 127.0.0.1 5001
         * ------------------------------------------------------------- */
        else if (strcmp(cmd, "updatetracker") == 0) {
            char filename[PEER_MAX_FILENAME];
            char ip[PEER_MAX_IP];
            long start_byte = 0;
            long end_byte = 0;
            int port = 0;

            memset(filename, 0, sizeof(filename));
            memset(ip, 0, sizeof(ip));

            /*
             * Try raw full format first.
             */
            int matched_full = sscanf(line,
                                      "%63s %255s %ld %ld %63s %d",
                                      cmd, filename, &start_byte,
                                      &end_byte, ip, &port);

            if (matched_full == 6) {
                handle_cmd_updatetracker_full(ctx,
                                              filename,
                                              start_byte,
                                              end_byte,
                                              ip,
                                              port);
                continue;
            }

            /*
             * Otherwise try the friendly short format.
             */
            if (sscanf(line, "%63s %255s %ld %ld",
                       cmd, filename, &start_byte, &end_byte) == 4) {
                handle_cmd_updatetracker(ctx, filename, start_byte, end_byte);
                continue;
            }

            printf("Usage:\n");
            printf("  updatetracker <filename> <start> <end>\n");
            printf("  updatetracker <filename> <start> <end> <ip> <port>\n");
        }

        /* -------------------------------------------------------------
         * Friendly download command
         * ------------------------------------------------------------- */
        else if (strcmp(cmd, "download") == 0) {
            char filename[PEER_MAX_FILENAME];
            memset(filename, 0, sizeof(filename));

            if (sscanf(line, "%63s %255s", cmd, filename) != 2) {
                printf("Usage: download <filename>\n");
                continue;
            }

            handle_cmd_download(ctx, filename);
        }

        else if (strcmp(cmd, "help") == 0) {
            print_help();
        }

        else if (strcmp(cmd, "exit") == 0) {
            ctx->running = 0;
            break;
        }

        else {
            printf("Unknown command. Type 'help'.\n");
        }
    }
}


/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    PeerContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    /*
     * Load tracker-side client config:
     *   tracker port
     *   tracker IP
     *   update interval
     */
    if (load_client_config("clientThreadConfig.cfg", &ctx.client_cfg) != 0) {
        fprintf(stderr, "[PEER] Failed to load clientThreadConfig.cfg\n");
        return 1;
    }

    /*
     * Load local peer server config:
     *   listen port
     *   shared directory
     */
    if (load_server_config("serverThreadConfig.cfg", &ctx.server_cfg) != 0) {
        fprintf(stderr, "[PEER] Failed to load serverThreadConfig.cfg\n");
        return 1;
    }

    /*
     * Local runtime directories used by this peer.
     */
    snprintf(ctx.cache_dir, sizeof(ctx.cache_dir), "cache");
    snprintf(ctx.state_dir, sizeof(ctx.state_dir), "state");
    snprintf(ctx.downloads_dir, sizeof(ctx.downloads_dir), "downloads");

    ensure_dir(ctx.server_cfg.shared_dir);
    ensure_dir(ctx.cache_dir);
    ensure_dir(ctx.state_dir);
    ensure_dir(ctx.downloads_dir);

    ctx.running = 1;

    pthread_mutex_init(&ctx.download_lock, NULL);
    pthread_mutex_init(&ctx.state_lock, NULL);

    print_peer_config(&ctx);

    /*
     * Start peer server thread.
     * This thread listens for incoming GETCHUNK requests from other peers.
     */
    PeerServerArgs server_args;
    memset(&server_args, 0, sizeof(server_args));
    server_args.ctx = &ctx;
    server_args.client_fd = -1;

    pthread_t server_tid;
    if (pthread_create(&server_tid, NULL, peer_server_main, &server_args) != 0) {
        fprintf(stderr, "[PEER] Failed to start peer server thread\n");
        ctx.running = 0;
        return 1;
    }

    /*
     * Start periodic tracker update thread.
     * This thread regularly tells the tracker what full files this peer has.
     */
    PeriodicUpdateArgs update_args;
    memset(&update_args, 0, sizeof(update_args));
    update_args.ctx = &ctx;

    pthread_t update_tid;
    if (pthread_create(&update_tid, NULL, periodic_update_main, &update_args) != 0) {
        fprintf(stderr, "[PEER] Failed to start periodic update thread\n");
        ctx.running = 0;
        pthread_cancel(server_tid);
        pthread_join(server_tid, NULL);
        return 1;
    }

    /*
     * On startup, try resuming incomplete downloads from state/.
     */
    resume_incomplete_downloads(&ctx);

    print_help();
    command_loop(&ctx);

    /*
     * Shutdown sequence.
     */
    ctx.running = 0;

    pthread_cancel(server_tid);
    pthread_cancel(update_tid);

    pthread_join(server_tid, NULL);
    pthread_join(update_tid, NULL);

    pthread_mutex_destroy(&ctx.download_lock);
    pthread_mutex_destroy(&ctx.state_lock);

    printf("[PEER] Shutdown complete.\n");
    return 0;
}