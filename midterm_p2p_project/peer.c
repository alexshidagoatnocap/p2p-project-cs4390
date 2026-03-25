#define _POSIX_C_SOURCE 200809L
#include "peer.h"
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
#include <unistd.h>

int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t total = 0;

    while (total < len) {
        ssize_t n = send(fd, p + total, len - total, 0);
        if (n <= 0) return -1;
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

static int load_client_config(const char *filename, ClientConfig *cfg) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen client config");
        return -1;
    }

    char line[256];
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return -1; }
    cfg->tracker_port = (uint16_t)strtoul(line, NULL, 10);

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return -1; }
    line[strcspn(line, "\r\n")] = '\0';
    strncpy(cfg->tracker_ip, line, sizeof(cfg->tracker_ip) - 1);
    cfg->tracker_ip[sizeof(cfg->tracker_ip) - 1] = '\0';

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return -1; }
    cfg->update_interval_seconds = (int)strtol(line, NULL, 10);

    fclose(fp);
    return 0;
}

static int load_server_config(const char *filename, ServerConfig *cfg) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen server config");
        return -1;
    }

    char line[256];
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return -1; }
    cfg->listen_port = (uint16_t)strtoul(line, NULL, 10);

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return -1; }
    line[strcspn(line, "\r\n")] = '\0';
    strncpy(cfg->shared_folder, line, sizeof(cfg->shared_folder) - 1);
    cfg->shared_folder[sizeof(cfg->shared_folder) - 1] = '\0';

    fclose(fp);
    return 0;
}

static int get_local_ip_for_remote(const char *remote_ip, uint16_t remote_port, char *out_ip, size_t out_sz) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(remote_port);

    if (inet_pton(AF_INET, remote_ip, &remote.sin_addr) != 1) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
        close(sock);
        return -1;
    }

    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &len) < 0) {
        close(sock);
        return -1;
    }

    if (!inet_ntop(AF_INET, &local.sin_addr, out_ip, out_sz)) {
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

static int get_file_size(const char *path, size_t *size_out) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    *size_out = (size_t)st.st_size;
    return 0;
}

static void fake_md5_for_demo(const char *filename, char out_md5[33]) {
    snprintf(out_md5, 33, "11111111111111111111111111111111");
    (void)filename;
}

static int tracker_request_response(const ClientConfig *client_cfg, const char *request, char *response, size_t response_sz) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(client_cfg->tracker_port);

    if (inet_pton(AF_INET, client_cfg->tracker_ip, &addr.sin_addr) != 1) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect tracker");
        close(sock);
        return -1;
    }

    if (send_all(sock, request, strlen(request)) != 0) {
        close(sock);
        return -1;
    }

    ssize_t n = recv(sock, response, response_sz - 1, 0);
    if (n < 0) {
        perror("recv tracker");
        close(sock);
        return -1;
    }

    response[n] = '\0';
    close(sock);
    return 0;
}

static int send_createtracker(const ClientConfig *client_cfg, const ServerConfig *server_cfg, const char *filename) {
    char path[PEER_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", server_cfg->shared_folder, filename);

    size_t filesize;
    if (get_file_size(path, &filesize) != 0) {
        printf("[PEER] File not found in shared folder: %s\n", path);
        return -1;
    }

    char md5[33];
    fake_md5_for_demo(filename, md5);

    char local_ip[64];
    if (get_local_ip_for_remote(client_cfg->tracker_ip, client_cfg->tracker_port, local_ip, sizeof(local_ip)) != 0) {
        strcpy(local_ip, "127.0.0.1");
    }

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request),
             "<createtracker %s %zu demo %s %s %u>\n",
             filename,
             filesize,
             md5,
             local_ip,
             server_cfg->listen_port);

    char response[BUFFER_SIZE];
    if (tracker_request_response(client_cfg, request, response, sizeof(response)) != 0) {
        return -1;
    }

    printf("[PEER] Tracker response:\n%s\n", response);
    return 0;
}

static int send_updatetracker(const ClientConfig *client_cfg, const ServerConfig *server_cfg, const char *filename, uint32_t start_byte, uint32_t end_byte) {
    char local_ip[64];
    if (get_local_ip_for_remote(client_cfg->tracker_ip, client_cfg->tracker_port, local_ip, sizeof(local_ip)) != 0) {
        strcpy(local_ip, "127.0.0.1");
    }

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request),
             "<updatetracker %s %u %u %s %u>\n",
             filename,
             start_byte,
             end_byte,
             local_ip,
             server_cfg->listen_port);

    char response[BUFFER_SIZE];
    if (tracker_request_response(client_cfg, request, response, sizeof(response)) != 0) {
        return -1;
    }

    printf("[PEER] Tracker response:\n%s\n", response);
    return 0;
}

static int send_req_list(const ClientConfig *client_cfg) {
    char response[BUFFER_SIZE * 4];
    if (tracker_request_response(client_cfg, "<REQ LIST>\n", response, sizeof(response)) != 0) {
        return -1;
    }
    printf("[PEER] LIST response:\n%s\n", response);
    return 0;
}

static int send_get_track(const ClientConfig *client_cfg, const char *track_filename) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(client_cfg->tracker_port);

    if (inet_pton(AF_INET, client_cfg->tracker_ip, &addr.sin_addr) != 1) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect tracker");
        close(sock);
        return -1;
    }

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "<GET %s>\n", track_filename);
    if (send_all(sock, request, strlen(request)) != 0) {
        close(sock);
        return -1;
    }

    ensure_directory_exists("cache");
    char cache_path[PEER_MAX_PATH];
    snprintf(cache_path, sizeof(cache_path), "cache/%s", track_filename);

    FILE *fp = fopen(cache_path, "w");
    if (!fp) {
        perror("fopen cache");
        close(sock);
        return -1;
    }

    char buf[BUFFER_SIZE];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, (size_t)n, fp);
    }

    fclose(fp);
    close(sock);

    printf("[PEER] Saved tracker response to %s\n", cache_path);
    return 0;
}

static void *peer_file_server_thread(void *arg) {
    ServerConfig *cfg = (ServerConfig *)arg;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("peer socket");
        return NULL;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg->listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("peer bind");
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 16) < 0) {
        perror("peer listen");
        close(server_fd);
        return NULL;
    }

    printf("[PEER SERVER] Listening on port %u\n", cfg->listen_port);
    printf("[PEER SERVER] Shared folder: %s\n", cfg->shared_folder);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
        if (client_fd < 0) {
            perror("peer accept");
            continue;
        }

        char line[BUFFER_SIZE];
        ssize_t n = recv_line(client_fd, line, sizeof(line));
        if (n <= 0) {
            close(client_fd);
            continue;
        }

        printf("[PEER SERVER] Received request: %s", line);

        char cmd[64], filename[256];
        memset(cmd, 0, sizeof(cmd));
        memset(filename, 0, sizeof(filename));

        if (sscanf(line, "%63s %255s", cmd, filename) == 2 && strcmp(cmd, "FILEGET") == 0) {
            char path[PEER_MAX_PATH];
            snprintf(path, sizeof(path), "%s/%s", cfg->shared_folder, filename);

            FILE *fp = fopen(path, "rb");
            if (!fp) {
                send_all(client_fd, "FILEERR\n", 8);
                close(client_fd);
                continue;
            }

            size_t filesize;
            if (get_file_size(path, &filesize) != 0) {
                fclose(fp);
                send_all(client_fd, "FILEERR\n", 8);
                close(client_fd);
                continue;
            }

            char header[128];
            snprintf(header, sizeof(header), "FILEBEGIN %zu\n", filesize);
            send_all(client_fd, header, strlen(header));

            char buf[CHUNK_SIZE];
            size_t bytes;
            while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0) {
                if (send_all(client_fd, buf, bytes) != 0) {
                    break;
                }
            }

            fclose(fp);
        } else {
            send_all(client_fd, "FILEERR\n", 8);
        }

        close(client_fd);
    }

    close(server_fd);
    return NULL;
}

typedef struct {
    ClientConfig client_cfg;
    ServerConfig server_cfg;
} UpdateThreadArgs;

static int download_file_from_peer(const char *peer_ip, uint16_t peer_port, const char *filename, const char *save_dir) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer_port);

    if (inet_pton(AF_INET, peer_ip, &addr.sin_addr) != 1) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect peer");
        close(sock);
        return -1;
    }

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "FILEGET %s\n", filename);
    if (send_all(sock, request, strlen(request)) != 0) {
        close(sock);
        return -1;
    }

    char header[BUFFER_SIZE];
    if (recv_line(sock, header, sizeof(header)) <= 0) {
        close(sock);
        return -1;
    }

    if (strncmp(header, "FILEBEGIN ", 10) != 0) {
        printf("[PEER] Invalid response from peer: %s\n", header);
        close(sock);
        return -1;
    }

    size_t filesize = 0;
    sscanf(header + 10, "%zu", &filesize);

    ensure_directory_exists(save_dir);

    char outpath[PEER_MAX_PATH];
    snprintf(outpath, sizeof(outpath), "%s/%s", save_dir, filename);

    FILE *fp = fopen(outpath, "wb");
    if (!fp) {
        perror("fopen output");
        close(sock);
        return -1;
    }

    size_t received = 0;
    char buf[CHUNK_SIZE];
    while (received < filesize) {
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        fwrite(buf, 1, (size_t)n, fp);
        received += (size_t)n;
    }

    fclose(fp);
    close(sock);

    printf("[PEER] Downloaded file to %s (%zu bytes)\n", outpath, received);
    return (received == filesize) ? 0 : -1;
}

static void *periodic_update_thread(void *arg) {
    UpdateThreadArgs *args = (UpdateThreadArgs *)arg;

    while (1) {
        DIR *dir = opendir(args->server_cfg.shared_folder);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;

                char path[PEER_MAX_PATH];
                snprintf(path, sizeof(path), "%s/%s", args->server_cfg.shared_folder, entry->d_name);

                struct stat st;
                if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                    send_updatetracker(&args->client_cfg, &args->server_cfg, entry->d_name, 0, (uint32_t)st.st_size);
                }
            }
            closedir(dir);
        }

        sleep(args->client_cfg.update_interval_seconds);
    }

    return NULL;
}

static void command_loop(const ClientConfig *client_cfg, const ServerConfig *server_cfg) {
    char line[BUFFER_SIZE];

    printf("\nCommands:\n");
    printf("  createtracker <filename>\n");
    printf("  updatetracker <filename> <start> <end>\n");
    printf("  list\n");
    printf("  gettrack <filename.track>\n");
    printf("  download <filename> <peer_ip> <peer_port>\n");
    printf("  help\n");
    printf("  exit\n\n");

    while (1) {
        printf("peer> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        line[strcspn(line, "\r\n")] = '\0';

        if (strncmp(line, "createtracker ", 14) == 0) {
            char filename[256];
            if (sscanf(line + 14, "%255s", filename) == 1) {
                send_createtracker(client_cfg, server_cfg, filename);
            } else {
                printf("Usage: createtracker <filename>\n");
            }
        } else if (strncmp(line, "updatetracker ", 14) == 0) {
            char filename[256];
            unsigned int start, end;
            if (sscanf(line + 14, "%255s %u %u", filename, &start, &end) == 3) {
                send_updatetracker(client_cfg, server_cfg, filename, start, end);
            } else {
                printf("Usage: updatetracker <filename> <start> <end>\n");
            }
        } else if (strcmp(line, "list") == 0) {
            send_req_list(client_cfg);
        } else if (strncmp(line, "gettrack ", 9) == 0) {
            char trackfile[256];
            if (sscanf(line + 9, "%255s", trackfile) == 1) {
                send_get_track(client_cfg, trackfile);
            } else {
                printf("Usage: gettrack <filename.track>\n");
            }
        } else if (strncmp(line, "download ", 9) == 0) {
            char filename[256], ip[64];
            unsigned int port;
            if (sscanf(line + 9, "%255s %63s %u", filename, ip, &port) == 3) {
                download_file_from_peer(ip, (uint16_t)port, filename, server_cfg->shared_folder);
            } else {
                printf("Usage: download <filename> <peer_ip> <peer_port>\n");
            }
        } else if (strcmp(line, "help") == 0) {
            printf("Commands:\n");
            printf("  createtracker <filename>\n");
            printf("  updatetracker <filename> <start> <end>\n");
            printf("  list\n");
            printf("  gettrack <filename.track>\n");
            printf("  download <filename> <peer_ip> <peer_port>\n");
            printf("  help\n");
            printf("  exit\n");
        } else if (strcmp(line, "exit") == 0) {
            break;
        } else if (strlen(line) == 0) {
            continue;
        } else {
            printf("Unknown command. Type 'help'.\n");
        }
    }
}

int main(void) {
    ClientConfig client_cfg;
    ServerConfig server_cfg;
    memset(&client_cfg, 0, sizeof(client_cfg));
    memset(&server_cfg, 0, sizeof(server_cfg));

    if (load_client_config("clientThreadConfig.cfg", &client_cfg) != 0) {
        fprintf(stderr, "Failed to load clientThreadConfig.cfg\n");
        return 1;
    }

    if (load_server_config("serverThreadConfig.cfg", &server_cfg) != 0) {
        fprintf(stderr, "Failed to load serverThreadConfig.cfg\n");
        return 1;
    }

    if (ensure_directory_exists(server_cfg.shared_folder) != 0) {
        fprintf(stderr, "Failed to ensure shared folder exists: %s\n", server_cfg.shared_folder);
        return 1;
    }

    printf("[PEER] Tracker IP: %s\n", client_cfg.tracker_ip);
    printf("[PEER] Tracker Port: %u\n", client_cfg.tracker_port);
    printf("[PEER] Update Interval: %d sec\n", client_cfg.update_interval_seconds);
    printf("[PEER] Listen Port: %u\n", server_cfg.listen_port);
    printf("[PEER] Shared Folder: %s\n", server_cfg.shared_folder);

    pthread_t server_tid;
    if (pthread_create(&server_tid, NULL, peer_file_server_thread, &server_cfg) != 0) {
        perror("pthread_create server");
        return 1;
    }

    UpdateThreadArgs *uargs = malloc(sizeof(*uargs));
    if (!uargs) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    uargs->client_cfg = client_cfg;
    uargs->server_cfg = server_cfg;

    pthread_t update_tid;
    if (pthread_create(&update_tid, NULL, periodic_update_thread, uargs) != 0) {
        perror("pthread_create updater");
        free(uargs);
        return 1;
    }

    command_loop(&client_cfg, &server_cfg);

    printf("[PEER] Exiting.\n");
    return 0;
}
