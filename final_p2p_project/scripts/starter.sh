#!/bin/bash

set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

echo "======================================"
echo " FINAL P2P 13-PEER DEMO STARTER"
echo "======================================"

echo "[1/10] Cleaning old demo files..."
bash scripts/clean_demo.sh

echo "[2/10] Building code..."
make

echo "[3/10] Setting up peer folders..."
bash scripts/setup_peers.sh

echo "[4/10] Adding sample shared files to seed peers..."

# Small file for peer1
echo "Hello from peer1" > peer1/shared/hello.txt

# Larger file for peer2
# You should increase this if you want the download to last longer.
python3 - <<'PY'
with open("peer2/shared/bigfile.bin", "wb") as f:
    f.write(b"A" * 2000000)   # ~2 MB, increase more if needed
PY

echo "[5/10] Starting tracker..."
(
    cd tracker
    ./tracker > ../tracker.log 2>&1
) &
TRACKER_PID=$!
echo "[STARTER] Tracker PID = ${TRACKER_PID}"

sleep 2

echo "[6/10] Starting seed peers: peer1 and peer2..."

(
    cd peer1
    {
        echo "createtracker hello.txt"
        sleep 300
        echo "exit"
    } | ./peer > ../peer1.log 2>&1
) &
P1_PID=$!

(
    cd peer2
    {
        echo "createtracker bigfile.bin"
        sleep 300
        echo "exit"
    } | ./peer > ../peer2.log 2>&1
) &
P2_PID=$!

echo "[STARTER] peer1 PID = ${P1_PID}"
echo "[STARTER] peer2 PID = ${P2_PID}"

echo "[7/10] Waiting until time = 30 sec before starting peer3 to peer8..."
sleep 10

for i in 3 4 5 6 7 8
do
    (
        cd "peer${i}"
        {
            sleep 2
            echo "list"
            sleep 1
            echo "gettrack hello.txt.track"
            sleep 1
            echo "download hello.txt"
            sleep 1
            echo "gettrack bigfile.bin.track"
            sleep 1
            echo "download bigfile.bin"
            sleep 180
            echo "exit"
        } | ./peer > "../peer${i}.log" 2>&1
    ) &
    eval "P${i}_PID=$!"
    echo "[STARTER] peer${i} PID = $(eval echo \$P${i}_PID)"
done

echo "[8/10] Waiting until time = 1 min 30 sec before starting peer9 to peer13..."
sleep 10

for i in 9 10 11 12 13
do
    (
        cd "peer${i}"
        {
            sleep 2
            echo "list"
            sleep 1
            echo "gettrack hello.txt.track"
            sleep 1
            echo "download hello.txt"
            sleep 1
            echo "gettrack bigfile.bin.track"
            sleep 1
            echo "download bigfile.bin"
            sleep 180
            echo "exit"
        } | ./peer > "../peer${i}.log" 2>&1
    ) &
    eval "P${i}_PID=$!"
    echo "[STARTER] peer${i} PID = $(eval echo \$P${i}_PID)"
done

echo "[9/10] Terminating peer1 and peer2 after step 7..."
kill "${P1_PID}" 2>/dev/null || true
kill "${P2_PID}" 2>/dev/null || true
echo "Peer1 terminated"
echo "Peer2 terminated"

echo "[10/10] Demo running..."
echo
echo "Logs:"
echo "  tracker.log"
for i in {1..13}
do
    echo "  peer${i}.log"
done

echo
echo "Tracker files should appear in:"
echo "  tracker/torrents/"

echo
echo "Downloaded files should appear in downloader peers:"
for i in {3..13}
do
    echo "  peer${i}/downloads/"
done

echo
echo "To stop all processes:"
echo "  bash scripts/stop_all.sh"

echo
echo "Demo starter complete."