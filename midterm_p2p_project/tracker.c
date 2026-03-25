#define _POSIX_C_SOURCE 200809L
#include "tracker.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

TrackerInfo trackerArray_g[256];

static pthread_mutex_t g_tracker_file_mutex = PTHREAD_MUTEX_INITIALIZER;

int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t total = 0;

    while (total < len) {
        ssize_t n = send(fd, p + total, len - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

ssize_t recv_line(int fd, char *buf, size_t maxlen) {
    size_t total = 0;

    while (total + 1 < maxlen) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n < 0) return -1;
        if (n == 0) break;

        buf[total++] = c;
        if (c == '\n') break;
    }

    buf[total] = '\0';
    return (ssize_t)total;
}

static int ensure_directory_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return mkdir(path, 0755);
}

static int load_sconfig(const char *filename, uint16_t *port, char *torrents_dir, size_t torrents_dir_sz) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen sconfig");
        return -1;
    }

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    *port = (uint16_t)strtoul(line, NULL, 10);

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    line[strcspn(line, "\r\n")] = '\0';
    strncpy(torrents_dir, line, torrents_dir_sz - 1);
    torrents_dir[torrents_dir_sz - 1] = '\0';

    fclose(fp);
    return 0;
}

static void build_track_path(char *out, size_t out_sz, const char *torrents_dir, const char *filename) {
    snprintf(out, out_sz, "%s/%s.track", torrents_dir, filename);
}

static int read_track_metadata(const char *track_path, TrackerInfo *info) {
    FILE *fp = fopen(track_path, "r");
    if (!fp) {
        return -1;
    }

    char line[512];
    memset(info, 0, sizeof(*info));

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Filename:", 9) == 0) {
            char *value = line + 9;
            while (*value == ' ') value++;
            value[strcspn(value, "\r\n")] = '\0';
            strncpy(info->filename, value, sizeof(info->filename) - 1);
        } else if (strncmp(line, "Filesize:", 9) == 0) {
            char *value = line + 9;
            while (*value == ' ') value++;
            info->filesize = (size_t)strtoull(value, NULL, 10);
        } else if (strncmp(line, "Description:", 12) == 0) {
            char *value = line + 12;
            while (*value == ' ') value++;
            value[strcspn(value, "\r\n")] = '\0';
            strncpy(info->description, value, sizeof(info->description) - 1);
        } else if (strncmp(line, "MD5:", 4) == 0) {
            char *value = line + 4;
            while (*value == ' ') value++;
            value[strcspn(value, "\r\n")] = '\0';
            strncpy(info->md5Hash, value, sizeof(info->md5Hash) - 1);
        }
    }

    fclose(fp);
    return 0;
}

static void handle_create_tracker(int client_fd, const char *torrents_dir, const CommandInfo *cmd) {
    char track_path[TRACKER_MAX_PATH];
    build_track_path(track_path, sizeof(track_path), torrents_dir, cmd->filename);

    pthread_mutex_lock(&g_tracker_file_mutex);

    FILE *check = fopen(track_path, "r");
    if (check) {
        fclose(check);
        pthread_mutex_unlock(&g_tracker_file_mutex);
        send_all(client_fd, "<createtracker ferr>\n", 22);
        return;
    }

    FILE *fp = fopen(track_path, "w");
    if (!fp) {
        pthread_mutex_unlock(&g_tracker_file_mutex);
        send_all(client_fd, "<createtracker fail>\n", 22);
        return;
    }

    time_t now = time(NULL);
    fprintf(fp, "Filename: %s\n", cmd->filename);
    fprintf(fp, "Filesize: %zu\n", cmd->filesize);
    fprintf(fp, "Description: %s\n", cmd->description);
    fprintf(fp, "MD5: %s\n", cmd->md5Hash);
    fprintf(fp, "# list of peers follows next\n");
    fprintf(fp, "%s:%u:%u:%zu:%ld\n",
            cmd->ip,
            cmd->port,
            0U,
            cmd->filesize,
            (long)now);

    fclose(fp);
    pthread_mutex_unlock(&g_tracker_file_mutex);

    send_all(client_fd, "<createtracker succ>\n", 22);
}

static void handle_update_tracker(int client_fd, const char *torrents_dir, const CommandInfo *cmd) {
    char track_path[TRACKER_MAX_PATH];
    build_track_path(track_path, sizeof(track_path), torrents_dir, cmd->filename);

    pthread_mutex_lock(&g_tracker_file_mutex);

    FILE *fp = fopen(track_path, "r");
    if (!fp) {
        pthread_mutex_unlock(&g_tracker_file_mutex);
        char resp[512];
        snprintf(resp, sizeof(resp), "<updatetracker %s ferr>\n", cmd->filename);
        send_all(client_fd, resp, strlen(resp));
        return;
    }

    char *lines[1024];
    int line_count = 0;
    char line[1024];

    while (fgets(line, sizeof(line), fp) && line_count < 1024) {
        lines[line_count] = strdup(line);
        if (!lines[line_count]) {
            fclose(fp);
            pthread_mutex_unlock(&g_tracker_file_mutex);
            char resp[512];
            snprintf(resp, sizeof(resp), "<updatetracker %s fail>\n", cmd->filename);
            send_all(client_fd, resp, strlen(resp));
            return;
        }
        line_count++;
    }
    fclose(fp);

    fp = fopen(track_path, "w");
    if (!fp) {
        for (int i = 0; i < line_count; i++) free(lines[i]);
        pthread_mutex_unlock(&g_tracker_file_mutex);
        char resp[512];
        snprintf(resp, sizeof(resp), "<updatetracker %s fail>\n", cmd->filename);
        send_all(client_fd, resp, strlen(resp));
        return;
    }

    int updated = 0;
    time_t now = time(NULL);

    for (int i = 0; i < line_count; i++) {
        char *cur = lines[i];

        if (cur[0] != '#' &&
            strncmp(cur, "Filename:", 9) != 0 &&
            strncmp(cur, "Filesize:", 9) != 0 &&
            strncmp(cur, "Description:", 12) != 0 &&
            strncmp(cur, "MD5:", 4) != 0) {

            char ip[64];
            unsigned int port = 0, start = 0, end = 0;
            long ts = 0;

            if (sscanf(cur, "%63[^:]:%u:%u:%u:%ld", ip, &port, &start, &end, &ts) == 5) {
                if (strcmp(ip, cmd->ip) == 0 && port == cmd->port) {
                    fprintf(fp, "%s:%u:%u:%u:%ld\n",
                            cmd->ip,
                            cmd->port,
                            cmd->startByte,
                            cmd->endByte,
                            (long)now);
                    updated = 1;
                    free(lines[i]);
                    continue;
                }
            }
        }

        fputs(cur, fp);
        free(lines[i]);
    }

    if (!updated) {
        fprintf(fp, "%s:%u:%u:%u:%ld\n",
                cmd->ip,
                cmd->port,
                cmd->startByte,
                cmd->endByte,
                (long)now);
    }

    fclose(fp);
    pthread_mutex_unlock(&g_tracker_file_mutex);

    char resp[512];
    snprintf(resp, sizeof(resp), "<updatetracker %s succ>\n", cmd->filename);
    send_all(client_fd, resp, strlen(resp));
}

static void handle_list(int client_fd, const char *torrents_dir) {
    DIR *dir = opendir(torrents_dir);
    if (!dir) {
        send_all(client_fd, "<REP LIST 0>\n<REP LIST END>\n", 29);
        return;
    }

    struct dirent *entry;
    TrackerInfo entries[512];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len >= 6 && strcmp(entry->d_name + len - 6, ".track") == 0) {
            char path[TRACKER_MAX_PATH];
            snprintf(path, sizeof(path), "%s/%s", torrents_dir, entry->d_name);
            TrackerInfo info;
            if (read_track_metadata(path, &info) == 0 && count < 512) {
                entries[count++] = info;
            }
        }
    }
    closedir(dir);

    char header[128];
    snprintf(header, sizeof(header), "<REP LIST %d>\n", count);
    send_all(client_fd, header, strlen(header));

    for (int i = 0; i < count; i++) {
        char out[512];
        snprintf(out, sizeof(out), "<%d %s %zu %s>\n", i + 1, entries[i].filename, entries[i].filesize, entries[i].md5Hash);
        send_all(client_fd, out, strlen(out));
    }

    send_all(client_fd, "<REP LIST END>\n", 15);
}

static void handle_get(int client_fd, const char *torrents_dir, const CommandInfo *cmd) {
    char fullpath[TRACKER_MAX_PATH];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", torrents_dir, cmd->filename);

    FILE *fp = fopen(fullpath, "r");
    if (!fp) {
        send_all(client_fd, "<REP GET BEGIN>\n<REP GET END FILE_NOT_FOUND>\n", 45);
        return;
    }

    TrackerInfo meta;
    memset(&meta, 0, sizeof(meta));
    read_track_metadata(fullpath, &meta);

    send_all(client_fd, "<REP GET BEGIN>\n", 16);

    char buf[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send_all(client_fd, buf, n) != 0) {
            fclose(fp);
            return;
        }
    }
    fclose(fp);

    char endline[256];
    snprintf(endline, sizeof(endline), "\n<REP GET END %s>\n", meta.md5Hash[0] ? meta.md5Hash : "UNKNOWN");
    send_all(client_fd, endline, strlen(endline));
}

static void *tracker_worker(void *arg) {
    TrackerWorkerArgs *w = (TrackerWorkerArgs *)arg;
    int client_fd = w->client_fd;
    char torrents_dir[TRACKER_MAX_PATH];
    strncpy(torrents_dir, w->torrents_dir, sizeof(torrents_dir) - 1);
    torrents_dir[sizeof(torrents_dir) - 1] = '\0';
    free(w);

    char line[BUFFER_SIZE];
    ssize_t n = recv_line(client_fd, line, sizeof(line));
    if (n <= 0) {
        close(client_fd);
        return NULL;
    }

    printf("[TRACKER] Received: %s", line);

    CommandInfo cmd = parseCommand(line);
    printf("[TRACKER] Parsed Type=%s Status=%s\n", commandTypeToString(cmd.Type), commandStatusToString(cmd.Status));

    if (cmd.Status == STATUS_FAIL) {
        send_all(client_fd, "<invalid command>\n", 18);
        close(client_fd);
        return NULL;
    }

    switch (cmd.Type) {
        case CMD_CREATE_TRACKER:
            handle_create_tracker(client_fd, torrents_dir, &cmd);
            break;
        case CMD_UPDATE_TRACKER:
            handle_update_tracker(client_fd, torrents_dir, &cmd);
            break;
        case CMD_LIST:
            handle_list(client_fd, torrents_dir);
            break;
        case CMD_GET:
            handle_get(client_fd, torrents_dir, &cmd);
            break;
        case CMD_EXIT:
            send_all(client_fd, "<bye>\n", 6);
            break;
        default:
            send_all(client_fd, "<invalid command>\n", 18);
            break;
    }

    close(client_fd);
    return NULL;
}

int main(void) {
    uint16_t port = 0;
    char torrents_dir[TRACKER_MAX_PATH];

    if (load_sconfig("sconfig", &port, torrents_dir, sizeof(torrents_dir)) != 0) {
        fprintf(stderr, "Failed to load sconfig\n");
        return 1;
    }

    if (ensure_directory_exists(torrents_dir) != 0) {
        fprintf(stderr, "Failed to ensure torrents directory exists: %s\n", torrents_dir);
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[TRACKER] Listening on port %u\n", port);
    printf("[TRACKER] Torrents directory: %s\n", torrents_dir);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        char ipbuf[64];
        inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));
        printf("[TRACKER] Connection from %s:%u\n", ipbuf, ntohs(client_addr.sin_port));

        TrackerWorkerArgs *args = malloc(sizeof(*args));
        if (!args) {
            close(client_fd);
            continue;
        }

        args->client_fd = client_fd;
        strncpy(args->torrents_dir, torrents_dir, sizeof(args->torrents_dir) - 1);
        args->torrents_dir[sizeof(args->torrents_dir) - 1] = '\0';

        pthread_t tid;
        if (pthread_create(&tid, NULL, tracker_worker, args) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(args);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
