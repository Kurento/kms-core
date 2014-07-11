add_test_program (test_memory_agnosticbin agnosticbin.c)
add_dependencies(test_memory_agnosticbin ${LIBRARY_NAME}plugins)
target_include_directories(test_memory_agnosticbin PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${GSTREAMER_CHECK_INCLUDE_DIRS})
target_link_libraries(test_memory_agnosticbin
                      ${GSTREAMER_LIBRARIES}
                      ${GSTREAMER_CHECK_LIBRARIES})
