CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -O2
LDFLAGS=-lpthread

OBJS=tracker.o config.o protocol.o trackfile.o

all: tracker

tracker: $(OBJS)
	$(CC) $(CFLAGS) -o tracker $(OBJS) $(LDFLAGS)

tracker.o: tracker.c tracker.h config.h protocol.h trackfile.h
config.o: config.c config.h tracker.h
protocol.o: protocol.c protocol.h tracker.h
trackfile.o: trackfile.c trackfile.h tracker.h

clean:
	rm -f tracker *.o