#!/bin/bash

set -e

# For normal local demo, we can keep this as 127.0.0.1.
# For multi-computer testing, we change this to the real IP of our computer.
# Example:
# ADVERTISED_IP="160.168.1.210"
ADVERTISED_IP="127.0.0.1"

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

    PORT=$((5000 + i))

    cat > "${PEER_DIR}/serverThreadConfig.cfg" <<EOF
${PORT}
shared
${ADVERTISED_IP}
EOF

    chmod +x "${PEER_DIR}/peer"
done

echo "[SETUP] Done."
echo "[SETUP] Peers created:"
for i in {1..13}
do
    echo "peer${i}"
done
