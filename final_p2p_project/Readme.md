P2P File Sharing System (Final Project)

Overview

This project implements a multi-threaded peer-to-peer (P2P) file sharing system using a centralized tracker.

Each peer can:
- Share files
- Download files from other peers
- Communicate with a tracker for metadata

The system supports:
- Multiple peers
- Chunk-based downloads
- Resume functionality
- Manual protocol testing


Project Structure

final_p2p_project/

tracker/          -> Tracker server  
peer/             -> Peer source code  
peer1...peer13/   -> Peer instances (created by setup script)  
scripts/          -> Automation scripts  
Makefile  
README.txt  


Build Instructions

From the project root:

make

This builds:
- tracker executable
- peer executable


Setup Peers

use command:
make setup    or
bash scripts/setup_peers.sh

This creates:
- peer1 to peer13 folders
- copies binaries and configs
- creates directories:
  shared/
  cache/
  state/
  downloads/


Running the Full Demo

bash scripts/starter.sh

This will:
1. Clean previous state
2. Start tracker
3. Start peer1 and peer2
4. After 30 seconds → start peer3–peer8
5. After 1 min 30 sec → start peer9–peer13
6. Peers download files automatically


Stop the Demo

bash scripts/stop_all.sh


Checking Results

Tracker files:
ls tracker/torrents

Expected:
hello.txt.track
bigfile.bin.track


Downloads:
ls peer3/downloads
ls peer8/downloads
ls peer13/downloads

Expected:
hello.txt
bigfile.bin


Logs:
cat peer3.log
cat tracker.log


Manual Testing (No Script)

Step 1: Start tracker

cd tracker
./tracker


Step 2: Start peer1

cd ../peer1
./peer

Run:
createtracker hello.txt


Step 3: Start peer2

cd ../peer2
./peer

Run:
REQ LIST
GET hello.txt.track
download hello.txt


Step 4: Verify

ls peer2/downloads

Expected:
hello.txt


Supported Commands

Friendly Commands:

createtracker <filename>
updatetracker <filename> <start> <end>
list
gettrack <filename.track>
download <filename>


Raw Protocol Commands:

REQ LIST
GET <filename.track>

createtracker <filename> <filesize> <description> <md5> <ip> <port>
updatetracker <filename> <start> <end> <ip> <port>


Key Features

- Multi-threaded peer server
- Tracker-based file discovery
- Chunk-based downloading
- Resume support using state files
- Periodic tracker updates
- Supports both friendly and raw protocol commands


Common Issues & Fixes

Address already in use:

bash scripts/stop_all.sh

or

pkill -f tracker


createtracker returns ferr:

bash scripts/clean_demo.sh


Connection refused:

Make sure tracker is running before peers.


File not downloading:

Check:
- file exists in peer1/shared/
- GET was run before download


Demo Explanation

“We start a tracker and two seed peers.
They register files using createtracker.
Other peers join, request metadata from the tracker,
and download files directly from peers using chunk-based transfer.”


Notes

- All peers run on localhost (127.0.0.1)
- Peer ports: 5001–5013
- Tracker port: 3490
- Files saved in downloads/


Conclusion

This project demonstrates:
- distributed file sharing
- centralized coordination with a tracker
- multi-peer communication
- concurrent downloads