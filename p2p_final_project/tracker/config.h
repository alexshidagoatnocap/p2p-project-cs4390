#ifndef TRACKER_CONFIG_H
#define TRACKER_CONFIG_H

#include "tracker.h"

TrackerStatus load_tracker_config(const char *path, TrackerConfig *config);
void print_tracker_config(const TrackerConfig *config);

#endif