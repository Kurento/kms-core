include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(KMS_SDP_AGENT_SOURCES
  kmssdpagentcommon.c
  kmssdpagent.c
  kmssdpcontext.c
  kmssdpmediahandler.c
  kmssdprtpmediahandler.c
  kmssdprtpavpfmediahandler.c
  kmssdprtpavpmediahandler.c
  kmssdprtpsavpmediahandler.c
  kmssdprtpsavpfmediahandler.c
  kmssdpsctpmediahandler.c
  kmsisdppayloadmanager.c
  kmssdppayloadmanager.c
  kmsisdpmediaextension.c
  kmssdpsdesext.c
)

set(KMS_SDP_AGENT_HEADERS
  kmssdpagentcommon.h
  kmssdpagent.h
  kmssdpcontext.h
  kmssdpmediahandler.h
  kmssdprtpmediahandler.h
  kmssdprtpavpfmediahandler.h
  kmssdprtpavpmediahandler.h
  kmssdprtpsavpmediahandler.h
  kmssdprtpsavpfmediahandler.h
  kmssdpsctpmediahandler.h
  kmsisdppayloadmanager.h
  kmssdppayloadmanager.h
  kmsisdpmediaextension.h
  kmssdpsdesext.h
)

add_glib_marshal(KMS_SDP_AGENT_SOURCES KMS_SDP_AGENT_HEADERS kms-sdp-agent-marshal __kms_sdp_agent_marshal)

add_library(kmssdpagent SHARED ${KMS_SDP_AGENT_SOURCES} ${KMS_SDP_AGENT_HEADERS})

add_dependencies(kmssdpagent kmsutils sdputils kmsrefstruct)

set_target_properties(kmssdpagent PROPERTIES PUBLIC_HEADER "${KMS_SDP_AGENT_HEADERS}")

set_target_properties(kmssdpagent PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

set(SDP_AGENT_INCLUDE_PREFIX "${INCLUDE_PREFIX}/sdpagent")

target_link_libraries(kmssdpagent
  ${gstreamer-1.5_LIBRARIES}
  ${gstreamer-base-1.5_LIBRARIES}
  ${gstreamer-sdp-1.5_LIBRARIES}
  ${gstreamer-pbutils-1.5_LIBRARIES}
  kmsutils
  sdputils
  kmsrefstruct
)

install(
  TARGETS kmssdpagent
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${SDP_AGENT_INCLUDE_PREFIX}
)

