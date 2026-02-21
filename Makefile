CXX = g++
CXXFLAGS = -Wall -Werror -Wextra -std=c++23
EXECDIR = exec
TARGETS = $(EXECDIR)/tracker $(EXECDIR)/peer

TRACKERSRC = $(wildcard tracker/*.cpp)
TRACKERBUILD = tracker/build
TRACKEROBJS = $(TRACKERSRC:tracker/%.cpp=$(TRACKERBUILD)/%.o)

PEERSRC = $(wildcard peer/*.cpp)
PEERBUILD = peer/build
PEEROBJS = $(PEERSRC:peer/%.cpp=$(PEERBUILD)/%.o)

all: clean dirs $(EXECDIR)/peer $(EXECDIR)/tracker

$(EXECDIR)/peer: $(PEEROBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(PEERBUILD)/%.o: $(PEERSRC)
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(EXECDIR)/tracker: $(TRACKEROBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TRACKERBUILD)/%.o: $(TRACKERSRC)
	$(CXX) $(CXXFLAGS) -c $^ -o $@

dirs:
	mkdir -p $(TRACKERBUILD) $(PEERBUILD) $(EXECDIR)

clean:
	rm -rf $(TRACKERBUILD) $(PEERBUILD) $(EXECDIR)

.PHONY: clean dirs all run tracker peer
