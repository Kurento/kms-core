set(KMS_SDP_AGENT_SOURCES
  kmssdpagent.c
  kmssdpcontext.c
  kmssdpmediahandler.c
  kmssdprtpmediahandler.c
  kmssdprtpavpfmediahandler.c
  kmssdprtpavpmediahandler.c
  kmssdprtpsavpfmediahandler.c
  kmssdpsctpmediahandler.c
  kmsisdppayloadmanager.c
  kmssdppayloadmanager.c
)

set(KMS_SDP_AGENT_HEADERS
  kmssdpagent.h
  kmssdpcontext.h
  kmssdpmediahandler.h
  kmssdprtpmediahandler.h
  kmssdprtpavpfmediahandler.h
  kmssdprtpavpmediahandler.h
  kmssdprtpsavpfmediahandler.h
  kmssdpsctpmediahandler.h
  kmsisdppayloadmanager.h
  kmssdppayloadmanager.h
)

add_library(kmssdpagent SHARED ${KMS_SDP_AGENT_SOURCES} ${KMS_SDP_AGENT_HEADERS})

add_dependencies(kmssdpagent kmsutils sdputils)

set_target_properties(kmssdpagent PROPERTIES PUBLIC_HEADER "${KMS_SDP_AGENT_HEADERS}")

set_target_properties(kmssdpagent PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

target_link_libraries(kmssdpagent
  ${gstreamer-1.5_LIBRARIES}
  ${gstreamer-base-1.5_LIBRARIES}
  ${gstreamer-sdp-1.5_LIBRARIES}
  ${gstreamer-pbutils-1.5_LIBRARIES}
  kmsutils
  sdputils
)

install(
  TARGETS kmssdpagent
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${INCLUDE_PREFIX}
)

