#ifndef PEER_SERVER_H
#define PEER_SERVER_H

#include "peer.h"

/*
 * Starts the peer server thread entry point.
 *
 * The peer server:
 *   - listens on ctx->server_cfg.listen_port
 *   - accepts incoming peer connections
 *   - handles GETCHUNK requests
 *   - enforces max chunk size of PEER_MAX_CHUNK_SIZE
 */
void *peer_server_main(void *arg);

#endif