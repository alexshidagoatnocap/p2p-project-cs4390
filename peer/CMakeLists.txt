add_executable(peer_app
  peer.c
)

target_include_directories(peer_app
  PRIVATE
  ../platform
)

message(STATUS "Building peer_app standalone with stub socket API")

find_package(Threads REQUIRED)

target_link_libraries(peer_app
  PRIVATE
  Threads::Threads
)
