set (SUPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/../valgrind.supp")

add_test_program (test_hubport hubport.c)
add_dependencies(test_hubport ${LIBRARY_NAME}plugins)
target_include_directories(test_hubport PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_hubport
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_filterelement filterelement.c)
add_dependencies(test_filterelement ${LIBRARY_NAME}plugins)
target_include_directories(test_filterelement PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_filterelement
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_agnosticbin agnosticbin.c)
add_dependencies(test_agnosticbin ${LIBRARY_NAME}plugins)
target_include_directories(test_agnosticbin PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_agnosticbin
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_audiomixerbin audiomixerbin.c)
add_dependencies(test_audiomixerbin ${LIBRARY_NAME}plugins)
target_include_directories(test_audiomixerbin PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_audiomixerbin
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

add_test_program (test_audiomixer audiomixer.c)
add_dependencies(test_audiomixer ${LIBRARY_NAME}plugins)
target_include_directories(test_audiomixer PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_audiomixer
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})

