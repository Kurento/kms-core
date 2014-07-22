include_directories(
  ${GSTREAMER_INCLUDE_DIRS}
  ${GSTREAMER_VIDEO_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set (SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp")

set (TEST_PROPERTIES
  GST_PLUGIN_PATH=${CMAKE_BINARY_DIR}
  CK_DEFAULT_TIMEOUT=50
  )

add_subdirectory(element)
add_subdirectory(general)

set (ENABLE_MEMORY_LEAKS_TESTS FALSE CACHE BOOL "Enable memory leaks tests")

if (${ENABLE_MEMORY_LEAKS_TESTS})
  add_subdirectory(memory_leaks)
endif()
