#!/bin/bash

set +e

echo "[CLEAN] Removing tracker logs and peer logs..."
rm -f tracker.log
for i in {1..13}
do
    rm -f "peer${i}.log"
done

echo "[CLEAN] Cleaning tracker torrents..."
mkdir -p tracker/torrents
rm -f tracker/torrents/*

echo "[CLEAN] Cleaning peer runtime folders..."
for i in {1..13}
do
    mkdir -p "peer${i}/shared"
    mkdir -p "peer${i}/cache"
    mkdir -p "peer${i}/state"
    mkdir -p "peer${i}/downloads"

    rm -f "peer${i}/cache/"*
    rm -f "peer${i}/state/"*
    rm -f "peer${i}/downloads/"*

    # keep shared dir, but remove old demo data
    rm -f "peer${i}/shared/"*
done

echo "[CLEAN] Done."