
if (${SOUP_FOUND} AND ${NICE_FOUND})
  include_directories(
    ${GSTREAMER_INCLUDE_DIRS}
    ${SOUP_INCLUDE_DIRS}
    ${NICE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../check
  )

  set(WEBRTC_HTTP_SERVER_SOURCES
    webrtc_http_server.c
  )

  add_executable (sample_webrtc_http_server ${WEBRTC_HTTP_SERVER_SOURCES})

  target_link_libraries(sample_webrtc_http_server
    kmstestutils
    ${GSTREAMER_LIBRARIES}
    ${GIO_LIBRARIES}
    ${SOUP_LIBRARIES}
    ${NICE_LIBRARIES}
  )

endif ()
