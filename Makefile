CC = g++
CFLAGS = -Wall -Werror -Wextra -std=c++23
BUILDDIR = build
EXECDIR = exec
SRCDIR = src
INCDIR = inc
SRCS = $(wildcard $(SRCDIR)/*.cpp)
OBJS = $(SRCS:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)
TARGET = $(EXECDIR)/p2p-network-cs4390

run:
	./exec/p2p-network-cs4390

all: clean dirs $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -c $^ -o $@

dirs:
	mkdir -p $(BUILDDIR) $(EXECDIR)

clean:
	rm -rf $(BUILDDIR) $(EXECDIR)

.PHONY: clean dirs all run
