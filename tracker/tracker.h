#pragma once

#include "api.h"
#include <stdint.h>

typedef enum { SOCKET_OK, SOCKET_ERROR } SocketStatus;

typedef struct {
  uint32_t acceptedFD;
  SocketAddress sockAddr;
  SocketStatus status;
} AcceptedSocket;
