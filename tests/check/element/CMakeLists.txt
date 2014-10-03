set (SUPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/../valgrind.supp")

add_test_program (test_hubport hubport.c)
add_dependencies(test_hubport ${LIBRARY_NAME}plugins)
target_include_directories(test_hubport PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS})
target_link_libraries(test_hubport
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES})

add_test_program (test_filterelement filterelement.c)
add_dependencies(test_filterelement ${LIBRARY_NAME}plugins)
target_include_directories(test_filterelement PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS})
target_link_libraries(test_filterelement
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES})

add_test_program (test_agnosticbin agnosticbin.c)
add_dependencies(test_agnosticbin ${LIBRARY_NAME}plugins)
target_include_directories(test_agnosticbin PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS})
target_link_libraries(test_agnosticbin
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES})

add_test_program (test_audiomixerbin audiomixerbin.c)
add_dependencies(test_audiomixerbin ${LIBRARY_NAME}plugins)
target_include_directories(test_audiomixerbin PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS})
target_link_libraries(test_audiomixerbin
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES})

add_test_program (test_audiomixer audiomixer.c)
add_dependencies(test_audiomixer ${LIBRARY_NAME}plugins)
target_include_directories(test_audiomixer PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS})
target_link_libraries(test_audiomixer
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES})

add_test_program (test_bufferinjector bufferinjector.c)
add_dependencies(test_bufferinjector ${LIBRARY_NAME}plugins)
target_include_directories(test_bufferinjector PRIVATE
                           ${gstreamer-1.0_INCLUDE_DIRS}
                           ${gstreamer-check-1.0_INCLUDE_DIRS})
target_link_libraries(test_bufferinjector
                      ${gstreamer-1.0_LIBRARIES}
                      ${gstreamer-check-1.0_LIBRARIES})
