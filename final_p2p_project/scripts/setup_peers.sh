#!/bin/bash

set -e

echo "[SETUP] Building tracker and peer..."
make -C tracker
make -C peer

echo "[SETUP] Preparing peer instances..."

for i in {1..13}
do
    PEER_DIR="peer${i}"

    mkdir -p "${PEER_DIR}"
    mkdir -p "${PEER_DIR}/shared"
    mkdir -p "${PEER_DIR}/cache"
    mkdir -p "${PEER_DIR}/state"
    mkdir -p "${PEER_DIR}/downloads"

    cp peer/peer "${PEER_DIR}/peer"
    cp peer/peer_template/clientThreadConfig.cfg "${PEER_DIR}/clientThreadConfig.cfg"
    cp peer/peer_template/serverThreadConfig.cfg "${PEER_DIR}/serverThreadConfig.cfg"

    PORT=$((5000 + i))

    cat > "${PEER_DIR}/serverThreadConfig.cfg" <<EOF
${PORT}
shared
EOF

    chmod +x "${PEER_DIR}/peer"
done

echo "[SETUP] Done."
echo "[SETUP] Peers created:"
for i in {1..13}
do
    echo "peer${i}"
done