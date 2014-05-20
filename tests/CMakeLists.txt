add_subdirectory(examples)

if (${GSTREAMER_CHECK_FOUND})
  add_subdirectory(check)
endif ()
