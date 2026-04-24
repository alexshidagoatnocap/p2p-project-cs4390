#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "peer_server.h"
#include "protocol.h"

/*
 * Build path to the file a peer can serve.
 *
 * We first look in shared_dir, then downloads_dir.
 * That way once a peer finishes downloading a file, it can also serve it.
 */
static int build_served_file_path(PeerContext *ctx,
                                  const char *filename,
                                  char *out_path,
                                  size_t out_size) {
    if (!ctx || !filename || !out_path || out_size == 0) {
        return -1;
    }

    char shared_path[PEER_MAX_PATH];
    snprintf(shared_path, sizeof(shared_path),
             "%s/%s", ctx->server_cfg.shared_dir, filename);

    struct stat st;
    if (stat(shared_path, &st) == 0) {
        snprintf(out_path, out_size, "%s", shared_path);
        return 0;
    }

    char downloads_path[PEER_MAX_PATH];
    snprintf(downloads_path, sizeof(downloads_path),
             "%s/%s", ctx->downloads_dir, filename);

    if (stat(downloads_path, &st) == 0) {
        snprintf(out_path, out_size, "%s", downloads_path);
        return 0;
    }

    return -1;
}

/*
 * Send a raw byte range from a file.
 *
 * Protocol:
 *   request:  <GETCHUNK filename start size>\n
 *   response success:
 *       <CHUNK OK filename start size>\n
 *       [exactly size raw bytes]
 *
 *   response invalid size:
 *       <GET invalid>\n
 *
 *   response failure:
 *       <CHUNK FAIL>\n
 */
static void handle_chunk_request(int client_fd,
                                 PeerContext *ctx,
                                 const char *filename,
                                 long start_byte,
                                 long chunk_size) {
    if (chunk_size <= 0 || chunk_size > PEER_MAX_CHUNK_SIZE) {
        peer_sendf(client_fd, "<GET invalid>\n");
        return;
    }

    char path[PEER_MAX_PATH];
    if (build_served_file_path(ctx, filename, path, sizeof(path)) != 0) {
        peer_sendf(client_fd, "<CHUNK FAIL>\n");
        return;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        peer_sendf(client_fd, "<CHUNK FAIL>\n");
        return;
    }

    /*
     * Make sure the requested range is actually available.
     */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        peer_sendf(client_fd, "<CHUNK FAIL>\n");
        return;
    }

    long filesize = ftell(fp);
    if (start_byte < 0 || start_byte >= filesize) {
        fclose(fp);
        peer_sendf(client_fd, "<CHUNK FAIL>\n");
        return;
    }

    /*
     * Clamp chunk size if the request extends beyond file end.
     */
    long available = filesize - start_byte;
    if (chunk_size > available) {
        chunk_size = available;
    }

    if (fseek(fp, start_byte, SEEK_SET) != 0) {
        fclose(fp);
        peer_sendf(client_fd, "<CHUNK FAIL>\n");
        return;
    }

    peer_sendf(client_fd, "<CHUNK OK %s %ld %ld>\n",
               filename, start_byte, chunk_size);

    char buffer[PEER_MAX_CHUNK_SIZE];
    size_t n = fread(buffer, 1, (size_t)chunk_size, fp);
    fclose(fp);

    if ((long)n != chunk_size) {
        peer_sendf(client_fd, "<CHUNK FAIL>\n");
        return;
    }

    peer_send_all(client_fd, buffer, n);
}

/*
 * One worker per connected peer client.
 */
static void *peer_server_worker(void *arg) {
    PeerServerArgs *args = (PeerServerArgs *)arg;
    PeerContext *ctx = args->ctx;
    int client_fd = args->client_fd;

    char line[PEER_MAX_LINE];
    int n = peer_recv_line(client_fd, line, sizeof(line));
    if (n <= 0) {
        close(client_fd);
        free(args);
        return NULL;
    }

    char filename[PEER_MAX_FILENAME];
    long start_byte = 0;
    long chunk_size = 0;

    if (parse_peer_chunk_request(line,
                                 filename,
                                 sizeof(filename),
                                 &start_byte,
                                 &chunk_size) != 0) {
        peer_sendf(client_fd, "<CHUNK FAIL>\n");
        close(client_fd);
        free(args);
        return NULL;
    }

    handle_chunk_request(client_fd, ctx, filename, start_byte, chunk_size);

    close(client_fd);
    free(args);
    return NULL;
}

void *peer_server_main(void *arg) {
    PeerServerArgs *server_args = (PeerServerArgs *)arg;
    PeerContext *ctx = server_args->ctx;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[PEER SERVER] socket");
        return NULL;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ctx->server_cfg.listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[PEER SERVER] bind");
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, PEER_BACKLOG) < 0) {
        perror("[PEER SERVER] listen");
        close(server_fd);
        return NULL;
    }

    printf("[PEER SERVER] Listening on port %d\n", ctx->server_cfg.listen_port);

    while (ctx->running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (!ctx->running) {
                break;
            }
            continue;
        }

        PeerServerArgs *worker_args = malloc(sizeof(PeerServerArgs));
        if (!worker_args) {
            close(client_fd);
            continue;
        }

        worker_args->ctx = ctx;
        worker_args->client_fd = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, peer_server_worker, worker_args) != 0) {
            close(client_fd);
            free(worker_args);
            continue;
        }

        pthread_detach(tid);
    }

    close(server_fd);
    return NULL;
}