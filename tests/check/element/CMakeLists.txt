set (SUPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/../valgrind.supp")

# all test programs
set(ALL_TESTS
  hubport
  filterelement
  agnosticbin
  agnosticbin3
  audiomixerbin
  audiomixer
  bufferinjector
  pad_connections
  passthrough
)

# tests targets
foreach(test ${ALL_TESTS})
  add_test_program (test_${test} ${test}.c)

  add_dependencies(test_${test} ${LIBRARY_NAME}plugins)

  target_include_directories(test_${test} PRIVATE
    ${gstreamer-1.0_INCLUDE_DIRS}
    ${gstreamer-check-1.0_INCLUDE_DIRS}
  )

  target_link_libraries(test_${test}
    ${gstreamer-1.0_LIBRARIES}
    ${gstreamer-check-1.0_LIBRARIES}
  )

endforeach(test)

#SDP Tests
add_test_program (test_sdp_agent sdp_agent.c)
add_dependencies(test_sdp_agent kmsgstcommons sdputils)
target_include_directories(test_sdp_agent PRIVATE
  ${gstreamer-1.0_INCLUDE_DIRS}
  ${gstreamer-check-1.0_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/gst-plugins/commons
)

target_link_libraries(test_sdp_agent
  ${gstreamer-1.0_LIBRARIES}
  ${gstreamer-check-1.0_LIBRARIES}
  kmsgstcommons
  sdputils
)

add_custom_target(clear_directory
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${KURENTO_DOT_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${KURENTO_DOT_DIR}
)

add_dependencies(check clear_directory)
