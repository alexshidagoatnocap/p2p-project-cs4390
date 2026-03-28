#pragma once

#include "protocol.h"

CommandStatus getAndSendTrackerInfo(TrackerInfo *tr, int32_t peerSockFD);
CommandStatus createTrackerFile(size_t trackerId);
CommandStatus updateTrackerFile(size_t trackerId);
