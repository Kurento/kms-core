set (SUPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/../valgrind.supp")

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
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_uriendpoint
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_recorderendpoint recorderendpoint.c)
add_dependencies(test_recorderendpoint kmsplugin)
target_include_directories(test_recorderendpoint PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_recorderendpoint
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_playerendpoint playerendpoint.c)
add_dependencies(test_playerendpoint kmstestutils kmsplugin)
target_include_directories(test_playerendpoint PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_playerendpoint
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_httpendpoint httpendpoint.c)
add_dependencies(test_httpendpoint kmsplugin)
target_include_directories(test_httpendpoint PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
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

add_test_program (test_sctp sctp.c)
add_dependencies(test_sctp kmsplugin)
target_include_directories(test_sctp PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_sctp
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_dispatcheronetomany dispatcheronetomany.c)
add_dependencies(test_dispatcheronetomany kmsplugin)
target_include_directories(test_dispatcheronetomany PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src"
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_dispatcheronetomany
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_audiomixerbin audiomixerbin.c)
add_dependencies(test_audiomixerbin kmsplugin)
target_include_directories(test_audiomixerbin PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_audiomixerbin
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_audiomixer audiomixer.c)
add_dependencies(test_audiomixer kmsplugin)
target_include_directories(test_audiomixer PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_audiomixer
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_compositemixer compositemixer.c)
add_dependencies(test_compositemixer kmsplugin)
target_include_directories(test_compositemixer PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src"
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_compositemixer
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_vp8parse vp8parse.c)
add_dependencies(test_vp8parse vp8parse)
target_include_directories(test_vp8parse PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_vp8parse
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_crowddetector crowddetector.c)
add_dependencies(test_crowddetector crowddetector kmsplugin)
target_include_directories(test_crowddetector PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_crowddetector
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_faceoverlay faceoverlay.c)
add_dependencies(test_faceoverlay faceoverlay kmsplugin)
target_include_directories(test_faceoverlay PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_faceoverlay
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_chroma chroma.c)
add_dependencies(test_chroma chroma kmsplugin kmsutils)
target_include_directories(test_chroma PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_chroma
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_pointerdetector pointerdetector.c)
add_dependencies(test_pointerdetector pointerdetector kmsplugin)
target_include_directories(test_pointerdetector PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_pointerdetector
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_pointerdetector2 pointerdetector.c)
add_dependencies(test_pointerdetector2 pointerdetector2 kmsplugin)
target_include_directories(test_pointerdetector2 PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_pointerdetector2
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_platedetector platedetector.c)
add_dependencies(test_platedetector platedetector kmsplugin)
target_include_directories(test_platedetector PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_platedetector
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)

add_test_program (test_jackvader jackvader.c)
add_dependencies(test_jackvader jackvader kmsplugin)
target_include_directories(test_jackvader PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/commons/")
target_link_libraries(test_jackvader
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES}
                      kmstestutils)
