#!/bin/sh

# This shell script exports a path containing the executables for the current shell process
#
# Source the environment variables by running
# $ source setup.sh
# after building the project.

export PATH="$PWD/build/peer:$PATH"
export PATH="$PWD/build/tracker:$PATH"
