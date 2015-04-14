add_test_program (test_memory_agnosticbin agnosticbin.c)
add_dependencies(test_memory_agnosticbin ${LIBRARY_NAME}plugins)
target_include_directories(test_memory_agnosticbin PRIVATE
                           ${gstreamer-1.5_INCLUDE_DIRS}
                           ${gstreamer-check-1.5_INCLUDE_DIRS})
target_link_libraries(test_memory_agnosticbin
                      ${gstreamer-1.5_LIBRARIES}
                      ${gstreamer-check-1.5_LIBRARIES})
