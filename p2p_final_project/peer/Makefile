CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -O2
LDFLAGS=-lpthread

OBJS=peer.o config.o protocol.o tracker_client.o state.o peer_server.o downloader.o

all: peer

peer: $(OBJS)
	$(CC) $(CFLAGS) -o peer $(OBJS) $(LDFLAGS)

peer.o: peer.c peer.h config.h protocol.h tracker_client.h state.h peer_server.h downloader.h
config.o: config.c config.h peer.h
protocol.o: protocol.c protocol.h peer.h
tracker_client.o: tracker_client.c tracker_client.h peer.h protocol.h
state.o: state.c state.h peer.h protocol.h
peer_server.o: peer_server.c peer_server.h peer.h protocol.h
downloader.o: downloader.c downloader.h peer.h protocol.h state.h tracker_client.h

clean:
	rm -f peer *.o