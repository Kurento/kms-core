set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

include(GLibHelpers)

add_subdirectory(commons)

set(KMS_CORE_SOURCES
  kmscore.c
  kmsagnosticbin.c kmsagnosticbin.h
  kmsagnosticbin3.c kmsagnosticbin3.h
#  kmsfilterelement.c kmsfilterelement.h
  kmsaudiomixer.c kmsaudiomixer.h
  kmsaudiomixerbin.c kmsaudiomixerbin.h
  kmsbitratefilter.c kmsbitratefilter.h
  kmsbufferinjector.c kmsbufferinjector.h
)

add_library(${LIBRARY_NAME}plugins MODULE ${KMS_CORE_SOURCES})

add_dependencies(${LIBRARY_NAME}plugins kmsgstcommons)

include_directories(
  ${gstreamer-1.0_INCLUDE_DIRS}
  ${gstreamer-base-1.0_SOURCE_DIRS}
  ${gstreamer-sdp-1.0_SOURCE_DIRS}
  ${gstreamer-pbutils-1.0_SOURCE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  "${CMAKE_CURRENT_BINARY_DIR}/commons/"
  "${CMAKE_CURRENT_SOURCE_DIR}/commons/"
)

target_link_libraries(${LIBRARY_NAME}plugins
  kmsgstcommons
  ${gstreamer-1.0_LIBRARIES}
  ${gstreamer-base-1.0_LIBRARIES}
  ${gstreamer-sdp-1.0_LIBRARIES}
  ${gstreamer-pbutils-1.0_LIBRARIES}
)

install(
  TARGETS ${LIBRARY_NAME}plugins
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_GST_PLUGINS_DIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

add_subdirectory (vp8parse)
