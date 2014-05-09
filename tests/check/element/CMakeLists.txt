add_test_program (test_rtpendpoint rtpendpoint.c)
add_dependencies(test_rtpendpoint kmstestutils kmsplugin)
target_include_directories(test_rtpendpoint PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_rtpendpoint
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_rtpendpoint_audio rtpendpoint_audio.c)
add_dependencies(test_rtpendpoint_audio kmstestutils kmsplugin)
target_include_directories(test_rtpendpoint_audio PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_rtpendpoint_audio
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_rtpendpoint_video rtpendpoint_video.c)
add_dependencies(test_rtpendpoint_video kmstestutils kmsplugin)
target_include_directories(test_rtpendpoint_video PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_rtpendpoint_video
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_webrtcendpoint webrtcendpoint.c)
add_dependencies(test_webrtcendpoint kmstestutils kmsplugin)
target_include_directories(test_webrtcendpoint PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_webrtcendpoint
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_mixerport mixerport.c)
add_dependencies(test_mixerport kmsplugin)
target_include_directories(test_mixerport PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_mixerport
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_filterelement filterelement.c)
add_dependencies(test_filterelement kmsplugin)
target_include_directories(test_filterelement PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_filterelement
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

if (${ENABLE_AUTOMUXER_TESTS})
add_test_program (test_automuxerbin automuxerbin.c)
add_dependencies(test_automuxerbin kmsplugin)
target_include_directories(test_automuxerbin PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_automuxerbin
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})
endif()

add_test_program (test_uriendpoint uriendpoint.c)
add_dependencies(test_uriendpoint kmsplugin)
target_include_directories(test_uriendpoint PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src")
target_link_libraries(test_uriendpoint
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_recorderendpoint recorderendpoint.c)
add_dependencies(test_recorderendpoint kmsplugin)
target_include_directories(test_recorderendpoint PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src")
target_link_libraries(test_recorderendpoint
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_playerendpoint playerendpoint.c)
add_dependencies(test_playerendpoint kmstestutils kmsplugin)
target_include_directories(test_playerendpoint PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src")
target_link_libraries(test_playerendpoint
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_httpendpoint httpendpoint.c)
add_dependencies(test_httpendpoint kmsplugin)
target_include_directories(test_httpendpoint PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src")
target_link_libraries(test_httpendpoint
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_agnosticbin agnosticbin.c)
add_dependencies(test_agnosticbin kmsplugin)
target_include_directories(test_agnosticbin PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_agnosticbin
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})
