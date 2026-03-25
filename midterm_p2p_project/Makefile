CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -pthread -O2

all: tracker peer

tracker: tracker.c protocol.c
	$(CC) $(CFLAGS) -o tracker tracker.c protocol.c

peer: peer.c protocol.c
	$(CC) $(CFLAGS) -o peer peer.c protocol.c

clean:
	rm -f tracker peer
