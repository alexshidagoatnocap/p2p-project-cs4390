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

rebuild: clean $(EXECDIR)/peer $(EXECDIR)/tracker

all: $(EXECDIR)/peer $(EXECDIR)/tracker

tracker: $(EXECDIR)/tracker
	./exec/tracker

peer: $(EXECDIR)/peer
	./exec/peer

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
	rm -rf $(TRACKERBUILD)/*.o $(PEERBUILD)/*.o $(EXECDIR)/*

.PHONY: clean dirs all run tracker peer
