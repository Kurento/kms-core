add_test_program (test_dummytest dummytest.c)
target_include_directories(test_dummytest PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS})
target_link_libraries(test_dummytest
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES})

add_test_program (test_sdp sdp.c)
add_dependencies(test_sdp ${LIBRARY_NAME}plugins kmsgstcommons)
target_include_directories(test_sdp PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/gst-plugins/commons/")
target_link_libraries(test_sdp
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES}
                      sdputils
                      kmsgstcommons)

add_test_program (test_utils utils.c)
add_dependencies(test_utils ${LIBRARY_NAME}plugins kmsgstcommons)
target_include_directories(test_utils PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/gst-plugins/commons/")
target_link_libraries(test_utils
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES}
                      kmsgstcommons)

add_test_program (test_refcounts refcounts.c)
add_dependencies(test_refcounts ${LIBRARY_NAME}plugins kmsgstcommons)
target_include_directories(test_refcounts PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/gst-plugins/commons/")
target_link_libraries(test_refcounts
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES}
                      kmsgstcommons)


