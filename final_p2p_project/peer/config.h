#ifndef PEER_CONFIG_H
#define PEER_CONFIG_H

#include "peer.h"

/*
 * Load tracker-side client configuration.
 * Expected file format:
 *   line 1: tracker port
 *   line 2: tracker IP
 *   line 3: update interval seconds
 */
int load_client_config(const char *path, ClientConfig *cfg);

/*
 * Load peer server configuration.
 * Expected file format:
 *   line 1: listen port
 *   line 2: shared directory name
 */
int load_server_config(const char *path, ServerConfig *cfg);

/*
 * Print loaded peer configuration.
 */
void print_peer_config(const PeerContext *ctx);

#endif