include_directories(
  ${gstreamer-1.0_INCLUDE_DIRS}
  ${gstreamer-base-1.0_INCLUDE_DIRS}
  ${gstreamer-video-1.0_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${VPX_INCLUDE_DIRS}
)

set(VP8PARSE_SOURCES
  vp8parse.c
  kmsvp8parse.c kmsvp8parse.h
)

add_library(vp8parse MODULE ${VP8PARSE_SOURCES})

target_link_libraries(vp8parse
  ${gstreamer-1.0_LIBRARIES}
  ${gstreamer-base-1.0_LIBRARIES}
  ${gstreamer-video-1.0_LIBRARIES}
  ${VPX_LIBRARIES}
)

install(
  TARGETS vp8parse
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_GST_PLUGINS_DIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
