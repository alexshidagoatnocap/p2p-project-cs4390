add_executable(peer_app
  peer.c
)

target_include_directories(peer_app
  PRIVATE
  ../platform
)

message(STATUS "Building peer_app standalone with stub socket API")

target_link_libraries(peer_app
  PRIVATE
  protocol_lib
)
