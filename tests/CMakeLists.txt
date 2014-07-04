include (TestHelpers)

if (${GSTREAMER_CHECK_FOUND})
  add_subdirectory(check)
endif ()

add_subdirectory(server)
