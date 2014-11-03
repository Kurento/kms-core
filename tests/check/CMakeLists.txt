include_directories(
  ${gstreamer-1.0_INCLUDE_DIRS}
  ${gstreamer-video-1.0_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set (SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp")
set (KURENTO_DOT_DIR "${CMAKE_BINARY_DIR}/debug/dots" CACHE PATH "Sets the directory where dot files will be dumped")

set (TEST_PROPERTIES
  GST_PLUGIN_PATH=${CMAKE_BINARY_DIR}
  GST_DEBUG_DUMP_DOT_DIR=${KURENTO_DOT_DIR}
  CK_DEFAULT_TIMEOUT=50
  )

set (VALGRING_TEST_PROPERTIES
  ${TEST_PROPERTIES}
  CK_DEFAULT_TIMEOUT=500
  )

add_subdirectory(element)
add_subdirectory(general)

set (ENABLE_MEMORY_LEAKS_TESTS FALSE CACHE BOOL "Enable memory leaks tests")

if (${ENABLE_MEMORY_LEAKS_TESTS})
  add_subdirectory(memory_leaks)
endif()
