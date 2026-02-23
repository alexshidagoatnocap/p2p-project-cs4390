cmake_minimum_required(VERSION 3.10)

project(p2p_network
  DESCRIPTION "Networks project for CS4390"
  LANGUAGES C
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(protocol)
add_subdirectory(tracker)
add_subdirectory(peer)
