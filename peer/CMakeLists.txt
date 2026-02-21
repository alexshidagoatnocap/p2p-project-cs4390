add_executable(peer_app
  peer.cpp
)

target_link_libraries(peer_app
  PRIVATE
  protocol_lib
)
