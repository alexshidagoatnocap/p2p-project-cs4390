# Peer-to-Peer Network Project

This is a Peer-to-Peer network implementation using sockets on Linux for CS4390.

## Build Instructions

To build the program, you will need

- A Linux-based operating system

- A GCC/G++ 15+

- CMake 3.20+

- GNU Make

- (Optional) Ninja

After you have cloned the repo, navigate to the project root directory.

Then, generate the build files by running

```sh
cmake -S . -B build
```

If you have Ninja installed, set the build generator to Ninja by running

```sh
cmake -S . -B build -G Ninja
```

If you would like to set the build generator to Ninja, delete the `build/` directory
and then run the command above.

Build the project by running

```sh
cmake --build build
```

To run the programs, source the path variables in `setup.sh` by running

```sh
source setup.sh
```

Then, you should be able to run the peer and tracker programs by running

```sh
peer_app
```

Or

```sh
tracker_app
```
