Peer-to-Peer File Sharing System (Midterm Demo)
Overview

This project implements a simplified peer-to-peer file sharing system with a centralized tracker.

The system consists of:

A tracker server
Multiple peers

Peers can:

Register files with the tracker
Request a list of available files
Download metadata (.track files)
Download actual files from other peers
Project Structure
midterm_p2p_project/
│
├── tracker
├── peer
├── protocol.c / protocol.h
├── tracker.c / tracker.h
├── peer.c / peer.h
├── Makefile
├── sconfig
├── torrents/
│
├── peer1/
│   ├── peer
│   ├── clientThreadConfig.cfg
│   ├── serverThreadConfig.cfg
│   ├── shared/
│   └── cache/
│
├── peer2/
│   ├── peer
│   ├── clientThreadConfig.cfg
│   ├── serverThreadConfig.cfg
│   ├── shared/
│   └── cache/
Compilation

From the main project directory:

make

If needed:

make clean
make
Fix Permissions (Linux/WSL)

If you see "Permission denied", run:

chmod +x tracker peer
chmod +x peer1/peer peer2/peer peer3/peer
Configuration
Tracker config (sconfig)
3490
torrents
Port: 3490
Tracker files stored in torrents/
Peer config
clientThreadConfig.cfg
3490
127.0.0.1
30
Tracker port
Tracker IP
Update interval (seconds)
serverThreadConfig.cfg

Peer1:

5001
shared

Peer2:

5002
shared
Running the System (2 Machines or 1 Computer)
Machine A (Tracker + Peer1)
Terminal 1
cd midterm_p2p_project
./tracker
Terminal 2
cd peer1
./peer

Create a test file:

echo "hello from peer1" > shared/hello.txt

Register file:

createtracker hello.txt
Machine B (Peer2)
cd peer2
./peer

If using 2 machines:

Update clientThreadConfig.cfg with Machine A IP
Demo Commands

Inside peer2:

list
gettrack hello.txt.track
download hello.txt <peer1_ip> 5001

If same machine:

download hello.txt 127.0.0.1 5001
Verify Download
cat shared/hello.txt

Expected output:

hello from peer1
Protocol Commands
createtracker filename
updatetracker filename start end
list → sends REQ LIST to tracker
gettrack filename.track → sends GET request
download filename ip port → peer-to-peer file transfer
Notes
Tracker is multithreaded
Each peer runs:
a server thread (serves files)
a client interface (commands)
Files are transferred using TCP sockets
.track files store metadata and peer info
Known Limitations (Midterm Version)
No segmented multi-peer downloading
MD5 is simplified (not fully validated)
Basic error handling
Common Issues
Permission denied
chmod +x peer tracker
Cannot connect to tracker
Check tracker is running
Verify IP in config
File not found
Ensure file is inside shared/
Wrong command

Use:

download filename ip port
Demo Summary

Peer1:

createtracker hello.txt

Peer2:

list
gettrack hello.txt.track
download hello.txt <peer1_ip> 5001
Concept Explanation
Tracker stores metadata about files
Peers register themselves with tracker
Peers download files directly from other peers
Tracker is only used for coordination