set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

include(GLibHelpers)

add_subdirectory(commons)

set(KMS_CORE_SOURCES
  kmscore.c
  kmsagnosticbin.c kmsagnosticbin.h
  kmsfilterelement.c kmsfilterelement.h
  kmsuriendpoint.c kmsuriendpoint.h
  kmsaudiomixer.c kmsaudiomixer.h
  kmsaudiomixerbin.c kmsaudiomixerbin.h
)

add_library(kmscore MODULE ${KMS_CORE_SOURCES})

add_dependencies(kmscore kmscommons)

include_directories(
  ${GSTREAMER_INCLUDE_DIRS}
  ${GSTREAMER_BASE_SOURCE_DIRS}
  ${GSTREAMER_SDP_SOURCE_DIRS}
  ${GSTREAMER_PBUTILS_SOURCE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  "${CMAKE_CURRENT_BINARY_DIR}/commons/"
  "${CMAKE_CURRENT_SOURCE_DIR}/commons/"
)

target_link_libraries(kmscore
  kmscommons
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_BASE_LIBRARIES}
  ${GSTREAMER_SDP_LIBRARIES}
  ${GSTREAMER_PBUTILS_LIBRARIES}
)

install(
  TARGETS kmscore
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_MODULESDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
