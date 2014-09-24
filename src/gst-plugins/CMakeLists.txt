set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

include(GLibHelpers)

add_subdirectory(commons)

set(KMS_CORE_SOURCES
  kmscore.c
  kmsagnosticbin.c kmsagnosticbin.h
  kmsfilterelement.c kmsfilterelement.h
  kmsaudiomixer.c kmsaudiomixer.h
  kmsaudiomixerbin.c kmsaudiomixerbin.h
  kmsbitratefilter.c kmsbitratefilter.h
)

add_library(${LIBRARY_NAME}plugins MODULE ${KMS_CORE_SOURCES})

add_dependencies(${LIBRARY_NAME}plugins kmsgstcommons)

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

target_link_libraries(${LIBRARY_NAME}plugins
  kmsgstcommons
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_BASE_LIBRARIES}
  ${GSTREAMER_SDP_LIBRARIES}
  ${GSTREAMER_PBUTILS_LIBRARIES}
)

install(
  TARGETS ${LIBRARY_NAME}plugins
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_GST_PLUGINS_DIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

add_subdirectory (vp8parse)
