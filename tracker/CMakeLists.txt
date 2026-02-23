add_executable(tracker_app
  tracker.c
)

target_link_libraries(tracker_app
  PRIVATE
  protocol_lib
)

message(STATUS "Platform API is ${PLATFORM_API}")

if(PLATFORM_API STREQUAL "LINUX_API")
  target_include_directories(tracker_app
    PRIVATE
    ../platform/linux
  )
elseif(PLATFORM_API STREQUAL "WINDOWS_API")
  target_include_directories(tracker_app
    PRIVATE
    ../platform/windows
  )

endif()

target_include_directories(tracker_app
  PRIVATE
  ../protocol
)
