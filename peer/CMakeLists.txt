add_executable(peer_app
  peer.c
)

message(STATUS "Platform API is ${PLATFORM_API}")

if(PLATFORM_API STREQUAL "LINUX_API")
  target_include_directories(peer_app
    PRIVATE
    ../platform
    ../platform/linux
  )

  target_link_libraries(peer_app
    PRIVATE
    linux_api_lib
  )
elseif(PLATFORM_API STREQUAL "WINDOWS_API")
  target_include_directories(peer_app
    PRIVATE
    ../platform
    ../platform/windows
  )
  target_link_libraries(peer_app
    PRIVATE
    windows_api_lib
  )

endif()

target_include_directories(peer_app
  PRIVATE
  ../protocol
)
