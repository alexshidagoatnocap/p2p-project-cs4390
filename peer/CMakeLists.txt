add_executable(peer_app
  peer.c
)

target_include_directories(peer_app
  PRIVATE
  ../platform
)

message(STATUS "Platform API is ${PLATFORM_API}")

if(PLATFORM_API STREQUAL "LINUX_API")
  target_link_libraries(peer_app
    PRIVATE
    linux_api_lib
  )
elseif(PLATFORM_API STREQUAL "WINDOWS_API")
  target_link_libraries(peer_app
    PRIVATE
    windows_api_lib
  )
endif()

target_include_directories(peer_app
  PRIVATE
  ../protocol
)

target_link_libraries(peer_app
  PRIVATE
  protocol_lib
)
