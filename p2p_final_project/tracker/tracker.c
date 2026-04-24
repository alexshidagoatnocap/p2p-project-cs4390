#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>

#include "tracker.h"
#include "config.h"
#include "protocol.h"
#include "trackfile.h"

/*
 * Ensure the torrents directory exists.
 */
static void ensure_torrents_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        mkdir(path, 0777);
    }
}

/*
 * Handle createtracker request:
 *   - build .track file path
 *   - fail if already exists
 *   - initialize TrackFile from request
 *   - save to disk
 */
static TrackerStatus handle_create_tracker(int client_fd,
                                           const TrackerRequest *request,
                                           const TrackerConfig *config,
                                           pthread_mutex_t *trackfile_mutex) {
    char path[MAX_PATH_LEN];
    build_trackfile_path(config->torrents_dir, request->filename, path, sizeof(path));

    pthread_mutex_lock(trackfile_mutex);

    if (trackfile_exists(path)) {
        pthread_mutex_unlock(trackfile_mutex);

        char response[MAX_LINE];
        build_create_response(response, sizeof(response), TRACKER_ERR_EXISTS);
        tracker_send_all(client_fd, response, strlen(response));
        return TRACKER_ERR_EXISTS;
    }

    TrackFile track;
    TrackerStatus status = trackfile_init_from_create(request, &track);
    if (status != TRACKER_OK) {
        pthread_mutex_unlock(trackfile_mutex);

        char response[MAX_LINE];
        build_create_response(response, sizeof(response), TRACKER_ERR_INTERNAL);
        tracker_send_all(client_fd, response, strlen(response));
        return status;
    }

    status = save_trackfile(path, &track);

    pthread_mutex_unlock(trackfile_mutex);

    char response[MAX_LINE];
    build_create_response(response, sizeof(response), status);
    tracker_send_all(client_fd, response, strlen(response));

    return status;
}

/*
 * Handle updatetracker request:
 *   - load existing .track
 *   - remove dead peers
 *   - update existing peer or add new one
 *   - save file back
 */
static TrackerStatus handle_update_tracker(int client_fd,
                                           const TrackerRequest *request,
                                           const TrackerConfig *config,
                                           pthread_mutex_t *trackfile_mutex) {
    char path[MAX_PATH_LEN];
    build_trackfile_path(config->torrents_dir, request->filename, path, sizeof(path));

    pthread_mutex_lock(trackfile_mutex);

    if (!trackfile_exists(path)) {
        pthread_mutex_unlock(trackfile_mutex);

        char response[MAX_LINE];
        build_update_response(response, sizeof(response),
                              request->filename, TRACKER_ERR_NOT_FOUND);
        tracker_send_all(client_fd, response, strlen(response));
        return TRACKER_ERR_NOT_FOUND;
    }

    TrackFile track;
    TrackerStatus status = load_trackfile(path, &track);
    if (status != TRACKER_OK) {
        pthread_mutex_unlock(trackfile_mutex);

        char response[MAX_LINE];
        build_update_response(response, sizeof(response),
                              request->filename, TRACKER_ERR_INTERNAL);
        tracker_send_all(client_fd, response, strlen(response));
        return status;
    }

    long now = (long)time(NULL);

    /*
     * Remove dead peers before updating the current peer.
     */
    remove_dead_peers(&track, now, config->peer_timeout_seconds);

    status = update_or_add_peer(&track,
                                request->ip,
                                request->port,
                                request->start_byte,
                                request->end_byte,
                                now);

    if (status == TRACKER_OK) {
        status = save_trackfile(path, &track);
    }

    pthread_mutex_unlock(trackfile_mutex);

    char response[MAX_LINE];
    build_update_response(response, sizeof(response), request->filename, status);
    tracker_send_all(client_fd, response, strlen(response));

    return status;
}

/*
 * Handle REQ LIST:
 *   - iterate over all *.track files in torrents/
 *   - load header info
 *   - send:
 *       <REP LIST X>
 *       <1 filename size md5>
 *       ...
 *       <REP LIST END>
 */
static TrackerStatus handle_list_request(int client_fd,
                                         const TrackerConfig *config,
                                         pthread_mutex_t *trackfile_mutex) {
    pthread_mutex_lock(trackfile_mutex);

    DIR *dir = opendir(config->torrents_dir);
    if (!dir) {
        pthread_mutex_unlock(trackfile_mutex);
        tracker_sendf(client_fd, "<REP LIST 0>\n<REP LIST END>\n");
        return TRACKER_ERR_FILE;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len >= 6 && strcmp(entry->d_name + len - 6, ".track") == 0) {
            count++;
        }
    }

    rewinddir(dir);

    tracker_sendf(client_fd, "<REP LIST %d>\n", count);

    int index = 1;

    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (!(len >= 6 && strcmp(entry->d_name + len - 6, ".track") == 0)) {
            continue;
        }

        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", config->torrents_dir, entry->d_name);

        TrackFile track;
        if (load_trackfile(path, &track) == TRACKER_OK) {
            tracker_sendf(client_fd, "<%d %s %ld %s>\n",
                          index,
                          track.header.filename,
                          track.header.filesize,
                          track.header.md5);
            index++;
        }
    }

    tracker_sendf(client_fd, "<REP LIST END>\n");

    closedir(dir);
    pthread_mutex_unlock(trackfile_mutex);

    return TRACKER_OK;
}

/*
 * Handle GET filename.track
 *   - open the .track file
 *   - send:
 *       <REP GET BEGIN>
 *       [full tracker file content]
 *       <REP GET END FileMD5>
 *
 * Here we simply reuse the MD5 stored in the track header.
 */
static TrackerStatus handle_get_request(int client_fd,
                                        const TrackerRequest *request,
                                        const TrackerConfig *config,
                                        pthread_mutex_t *trackfile_mutex) {
    pthread_mutex_lock(trackfile_mutex);

    /*
     * request->filename already contains something like "hello.txt.track"
     * for GET commands.
     */
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", config->torrents_dir, request->filename);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        pthread_mutex_unlock(trackfile_mutex);

        char response[MAX_LINE];
        build_error_response(response, sizeof(response), "file_not_found");
        tracker_send_all(client_fd, response, strlen(response));
        return TRACKER_ERR_NOT_FOUND;
    }

    /*
     * Load track header too, so we can use header.md5 in REP GET END.
     */
    TrackFile track;
    TrackerStatus status = load_trackfile(path, &track);
    if (status != TRACKER_OK) {
        fclose(fp);
        pthread_mutex_unlock(trackfile_mutex);

        char response[MAX_LINE];
        build_error_response(response, sizeof(response), "trackfile_parse_error");
        tracker_send_all(client_fd, response, strlen(response));
        return status;
    }

    tracker_sendf(client_fd, "<REP GET BEGIN>\n");

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        tracker_send_all(client_fd, line, strlen(line));
    }

    tracker_sendf(client_fd, "<REP GET END %s>\n", track.header.md5);

    fclose(fp);
    pthread_mutex_unlock(trackfile_mutex);

    return TRACKER_OK;
}

/*
 * Worker thread for one connected peer.
 * Reads one request line, parses it, handles it, then closes.
 */
static void *tracker_worker(void *arg) {
    TrackerWorkerArgs *worker = (TrackerWorkerArgs *)arg;
    int client_fd = worker->client_fd;

    char line[MAX_LINE];
    int n = tracker_recv_line(client_fd, line, sizeof(line));
    if (n <= 0) {
        close(client_fd);
        free(worker);
        return NULL;
    }

    printf("[TRACKER] Received: %s", line);

    TrackerRequest request;
    TrackerStatus status = parse_tracker_request(line, &request);

    if (status != TRACKER_OK) {
        char response[MAX_LINE];
        build_error_response(response, sizeof(response), "parse_error");
        tracker_send_all(client_fd, response, strlen(response));
        close(client_fd);
        free(worker);
        return NULL;
    }

    switch (request.type) {
        case TRACKER_CMD_CREATE:
            handle_create_tracker(client_fd,
                                  &request,
                                  &worker->config,
                                  worker->trackfile_mutex);
            break;

        case TRACKER_CMD_UPDATE:
            handle_update_tracker(client_fd,
                                  &request,
                                  &worker->config,
                                  worker->trackfile_mutex);
            break;

        case TRACKER_CMD_LIST:
            handle_list_request(client_fd,
                                &worker->config,
                                worker->trackfile_mutex);
            break;

        case TRACKER_CMD_GET:
            handle_get_request(client_fd,
                               &request,
                               &worker->config,
                               worker->trackfile_mutex);
            break;

        default: {
            char response[MAX_LINE];
            build_error_response(response, sizeof(response), "unknown_command");
            tracker_send_all(client_fd, response, strlen(response));
            break;
        }
    }

    close(client_fd);
    free(worker);
    return NULL;
}

/*
 * Main tracker server.
 */
int main(void) {
    TrackerConfig config;
    TrackerStatus status = load_tracker_config("sconfig", &config);
    if (status != TRACKER_OK) {
        fprintf(stderr, "[TRACKER] Failed to load sconfig\n");
        return 1;
    }

    print_tracker_config(&config);
    ensure_torrents_dir(config.torrents_dir);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[TRACKER] socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[TRACKER] bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, TRACKER_BACKLOG) < 0) {
        perror("[TRACKER] listen");
        close(server_fd);
        return 1;
    }

    printf("[TRACKER] Listening on port %d\n", config.port);
    printf("[TRACKER] Torrents directory: %s\n", config.torrents_dir);

    pthread_mutex_t trackfile_mutex = PTHREAD_MUTEX_INITIALIZER;

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            perror("[TRACKER] accept");
            continue;
        }

        printf("[TRACKER] Connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        TrackerWorkerArgs *worker = malloc(sizeof(TrackerWorkerArgs));
        if (!worker) {
            close(client_fd);
            continue;
        }

        worker->client_fd = client_fd;
        worker->config = config;
        worker->trackfile_mutex = &trackfile_mutex;

        pthread_t tid;
        if (pthread_create(&tid, NULL, tracker_worker, worker) != 0) {
            perror("[TRACKER] pthread_create");
            close(client_fd);
            free(worker);
            continue;
        }

        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}