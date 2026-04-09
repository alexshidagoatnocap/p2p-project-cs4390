# Peer System Implementation

Intializes the peer modules and socket API

Assumes hard coded tracker at a specfic ip addresss in largefile.bin

Splits into 1KB segements using the create_file_segements() to be able to track in memory

Asks the tracker for peer list for this message LIST:<filename> (but not now since it it hasn't connected to tracker)
- If no peers come back, it inserts 3 peers manually

Sends a download thread to:
- Pick the next missing segment
- Look for a peer that claims to have the segment
- Downloads it
- Writes a .record file
- Notifies the tracker

Still need to:
- Fix the format tracker passer since peer sends LIST: <filename> in peer.c
- The peer and tracker have hard coded ports (need to merge)
- The tracker command handlers are placeholders to print messages
- The network functions at the bottom are stubbed so it doesn't build a real socket
- Loop back testing on local host will be 
- Need to be able to run 13 peers for the project
