include (TestHelpers)

if (${gstreamer-check-1.5_FOUND})
  add_subdirectory(check)
endif ()

add_subdirectory(server)
