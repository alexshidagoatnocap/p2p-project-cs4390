add_executable(tracker_app
  tracker.c
)

target_link_libraries(tracker_app
  PRIVATE
  protocol_lib
)
