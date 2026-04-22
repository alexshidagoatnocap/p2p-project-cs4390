#!/bin/bash

set +e

echo "[STOP] Killing tracker and peer processes..."
pkill -f "/tracker/tracker"
pkill -f "/peer[1-9]/peer"
pkill -f "/peer1[0-3]/peer"
pkill -f "/peer/peer"

echo "[STOP] Done."