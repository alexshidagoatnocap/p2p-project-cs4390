if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(STATUS "Linux Detected. Including Linux Socket API.")
  set(PLATFORM_API "LINUX_API" CACHE INTERNAL "")
  add_library(linux_api_lib
    STATIC
    linux_api.c
)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  message(STATUS "Windows Detected. Including WinSock API.")
  set(PLATFORM_API "WINDOWS_API" CACHE INTERNAL "")
  add_library(windows_api_lib
    STATIC
    windows_api.c
  )
else()
  message(STATUS "No platform detected! Using tests.")
  set(PLATFORM_API "TESTING" CACHE INTERNAL "")
endif()
