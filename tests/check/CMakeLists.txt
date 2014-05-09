include_directories(
  ${GSTREAMER_INCLUDE_DIRS}
  ${GSTREAMER_VIDEO_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set(KMS_TEST_UTILS_SOURCES
  kmstestutils.h kmstestutils.c
  )

add_library(kmstestutils ${KMS_TEST_UTILS_SOURCES})

target_link_libraries(kmstestutils
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_BASE_LIBRARIES}
  ${GSTREAMER_SDP_LIBRARIES}
  ${GSTREAMER_PBUTILS_LIBRARIES}
)

install(
  TARGETS kmstestutils
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

include (TestHelper)

set (TEST_PROPERTIES
  GST_PLUGIN_PATH=${CMAKE_BINARY_DIR}
  CK_DEFAULT_TIMEOUT=25
  )

add_subdirectory(element)
# add_subdirectory(general)
# add_subdirectory(integration)
# add_subdirectory(memory_leaks)
