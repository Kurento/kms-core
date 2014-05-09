
if (${SOUP_FOUND} AND ${NICE_FOUND})
  include_directories(
    ${GSTREAMER_INCLUDE_DIRS}
    ${SOUP_INCLUDE_DIRS}
    ${NICE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set(WEBRTC_HTTP_SERVER_SOURCES
    webrtc_http_server.c
  )

  add_executable (webrtc_http_server ${WEBRTC_HTTP_SERVER_SOURCES})

  target_link_libraries(webrtc_http_server
    ${GSTREAMER_LIBRARIES}
    ${GIO_LIBRARIES}
    ${SOUP_LIBRARIES}
    ${NICE_LIBRARIES}
  )

endif ()
