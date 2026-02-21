cmake_minimum_required(VERSION 3.22)

project(p2p_network
  DESCRIPTION "Networks project for CS4390"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(protocol)
add_subdirectory(tracker)
add_subdirectory(peer)
