#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"

/*
 * Remove whitespace from beginning and end of a string.
 *
 * This is used when reading lines from config files
 * so we don’t keep unwanted spaces or newline characters.
 */
static void trim_local(char *s) {
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
 * Load client (tracker-side) configuration from file.
 *
 * Expected format of file:
 *   line 1: tracker port
 *   line 2: tracker IP
 *   line 3: update interval (seconds)
 *
 * Example:
 *   3490
 *   127.0.0.1
 *   30
 */
int load_client_config(const char *path, ClientConfig *cfg) {
    if (!path || !cfg) {
        return -1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("[PEER CONFIG] client config fopen");
        return -1;
    }

    char line[PEER_MAX_LINE];

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    trim_local(line);
    cfg->tracker_port = atoi(line);

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    trim_local(line);
    strncpy(cfg->tracker_ip, line, sizeof(cfg->tracker_ip) - 1);
    cfg->tracker_ip[sizeof(cfg->tracker_ip) - 1] = '\0';

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    trim_local(line);
    cfg->update_interval_seconds = atol(line);

    fclose(fp);

    if (cfg->tracker_port <= 0 || cfg->tracker_port > 65535) {
        fprintf(stderr, "[PEER CONFIG] Invalid tracker port: %d\n", cfg->tracker_port);
        return -1;
    }

    if (strlen(cfg->tracker_ip) == 0) {
        fprintf(stderr, "[PEER CONFIG] Empty tracker IP\n");
        return -1;
    }

    if (cfg->update_interval_seconds <= 0) {
        fprintf(stderr, "[PEER CONFIG] Invalid update interval: %ld\n",
                cfg->update_interval_seconds);
        return -1;
    }

    return 0;
}


/*
 * Load server (peer-side) configuration.
 *
 * Expected format:
 *   line 1: peer listen port
 *   line 2: shared directory name
 *
 * Example:
 *   5001
 *   shared
 */
int load_server_config(const char *path, ServerConfig *cfg) {
    if (!path || !cfg) {
        return -1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("[PEER CONFIG] server config fopen");
        return -1;
    }

    char line[PEER_MAX_LINE];

    /*
     * line 1: peer listen port
     */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    trim_local(line);
    cfg->listen_port = atoi(line);

    /*
     * line 2: shared directory
     */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    trim_local(line);
    strncpy(cfg->shared_dir, line, sizeof(cfg->shared_dir) - 1);
    cfg->shared_dir[sizeof(cfg->shared_dir) - 1] = '\0';

    /*
     * line 3: advertised IP
     *
     * This is the IP other peers need touse to connect to this peer.
     *
     * For our local demo:
     *   127.0.0.1
     *
     *
     * If old config files only have 2 lines, default to 127.0.0.1
     * so old scripts still work.
     */
    if (fgets(line, sizeof(line), fp)) {
        trim_local(line);
        strncpy(cfg->advertised_ip, line, sizeof(cfg->advertised_ip) - 1);
        cfg->advertised_ip[sizeof(cfg->advertised_ip) - 1] = '\0';
    } else {
        strncpy(cfg->advertised_ip, "127.0.0.1", sizeof(cfg->advertised_ip) - 1);
        cfg->advertised_ip[sizeof(cfg->advertised_ip) - 1] = '\0';
    }

    fclose(fp);

    if (cfg->listen_port <= 0 || cfg->listen_port > 65535) {
        fprintf(stderr, "[PEER CONFIG] Invalid listen port: %d\n", cfg->listen_port);
        return -1;
    }

    if (strlen(cfg->shared_dir) == 0) {
        fprintf(stderr, "[PEER CONFIG] Empty shared directory\n");
        return -1;
    }

    if (strlen(cfg->advertised_ip) == 0) {
        fprintf(stderr, "[PEER CONFIG] Empty advertised IP\n");
        return -1;
    }

    return 0;
}

/*
 * Print all peer configuration values.
 *
 * This helps verify that:
 *   - configs were loaded correctly
 *   - paths and ports are correct
 */
void print_peer_config(const PeerContext *ctx) {
    if (!ctx) return;

    printf("[PEER CONFIG]\n");
    printf("  Tracker IP: %s\n", ctx->client_cfg.tracker_ip);
    printf("  Tracker Port: %d\n", ctx->client_cfg.tracker_port);
    printf("  Update Interval: %ld sec\n", ctx->client_cfg.update_interval_seconds);
    printf("  Listen Port: %d\n", ctx->server_cfg.listen_port);
    printf("  Advertised IP: %s\n", ctx->server_cfg.advertised_ip);
    printf("  Shared Dir: %s\n", ctx->server_cfg.shared_dir);
    printf("  Cache Dir: %s\n", ctx->cache_dir);
    printf("  State Dir: %s\n", ctx->state_dir);
    printf("  Downloads Dir: %s\n", ctx->downloads_dir);
}