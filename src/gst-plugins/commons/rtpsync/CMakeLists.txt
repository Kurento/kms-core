set(KMS_RTP_SYNC_SOURCES
  kmsrtpsynccontext.c
  kmsrtpsynchronizer.c
)

set(KMS_RTP_SYNC_HEADERS
  kmsrtpsynccontext.h
  kmsrtpsynchronizer.h
)

add_library(kmsrtpsync SHARED ${KMS_RTP_SYNC_SOURCES} ${KMS_RTP_SYNC_HEADERS})
set_target_properties(kmsrtpsync PROPERTIES PUBLIC_HEADER "${KMS_RTP_SYNC_HEADERS}")
set_target_properties(kmsrtpsync PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

target_link_libraries(kmsrtpsync
  ${gstreamer-1.5_LIBRARIES}
  ${gstreamer-rtp-1.5_LIBRARIES}
)

set_property (TARGET kmsrtpsync
  PROPERTY INCLUDE_DIRECTORIES
    ${gstreamer-1.5_INCLUDE_DIRS}
    ${gstreamer-rtp-1.5_INCLUDE_DIRS}
)

set(RTP_SYNC_INCLUDE_PREFIX "${INCLUDE_PREFIX}/rtpsync")

install(
  TARGETS kmsrtpsync
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${RTP_SYNC_INCLUDE_PREFIX}
)
