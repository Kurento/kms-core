set(TEST_VARIABLES
  "GST_PLUGIN_PATH=$ENV{GST_PLUGIN_PATH}:${CMAKE_BINARY_DIR}"
  "GST_DEBUG_DUMP_DOT_DIR=${KURENTO_DOT_DIR}"
)
set(VALGRIND_TEST_VARIABLES
  "DEBUG_MEDIASET=1"
)
set(SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp")

add_test_program(test_module_manager moduleManager.cpp)
add_dependencies(test_module_manager ${LIBRARY_NAME}module ${LIBRARY_NAME}impl)
set_property(TARGET test_module_manager
  PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_BINARY_DIR}/../../
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.5_INCLUDE_DIRS}
)
target_link_libraries(test_module_manager
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
)

add_test_program(test_media_set mediaSet.cpp)
add_dependencies(test_media_set ${LIBRARY_NAME}module ${LIBRARY_NAME}impl)
set_property(TARGET test_media_set
  PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_BINARY_DIR}/../../
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.5_INCLUDE_DIRS}
)
target_link_libraries(test_media_set
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
)

add_test_program(test_media_element mediaElement.cpp)
add_dependencies(test_media_element kmscoreplugins ${LIBRARY_NAME}impl kmsgstcommons)
set_property(TARGET test_media_element
  PROPERTY INCLUDE_DIRECTORIES
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gst-plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.5_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(test_media_element
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boost_LIBRARIES}
  kmsgstcommons
)

add_test_program(test_rtp_endpoint_cpp rtpEndpoint.cpp)
add_dependencies(test_rtp_endpoint_cpp kmscoreplugins ${LIBRARY_NAME}impl)
set_property(TARGET test_rtp_endpoint_cpp
  PROPERTY INCLUDE_DIRECTORIES
  ${KmsJsonRpc_INCLUDE_DIRS}
  ${sigc++-2.0_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gst-plugins
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
  ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
  ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
  ${glibmm-2.4_INCLUDE_DIRS}
  ${gstreamer-1.5_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  )
target_link_libraries(test_rtp_endpoint_cpp
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boost_LIBRARIES}
  )

add_test_program(test_sdp_endpoint sdpEndpoint.cpp)
add_dependencies(test_sdp_endpoint kmscoreplugins ${LIBRARY_NAME}impl)
set_property(TARGET test_sdp_endpoint
  PROPERTY INCLUDE_DIRECTORIES
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gst-plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.5_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(test_sdp_endpoint
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boost_LIBRARIES}
)

add_test_program(test_media_object mediaObject.cpp)
add_dependencies(test_media_object kmscoreplugins ${LIBRARY_NAME}impl)
set_property(TARGET test_media_object
  PROPERTY INCLUDE_DIRECTORIES
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gst-plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.5_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(test_media_object
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boost_LIBRARIES}
)

add_test_program(test_complex_type complexType.cpp)
add_dependencies(test_complex_type kmscoreplugins ${LIBRARY_NAME}impl)
set_property(TARGET test_complex_type
  PROPERTY INCLUDE_DIRECTORIES
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.5_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(test_complex_type
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boost_LIBRARIES}
)

add_test_program(test_uri_endpoint uriEndpoint.cpp)
add_dependencies(test_uri_endpoint kmscoreplugins ${LIBRARY_NAME}impl)
set_property(TARGET test_uri_endpoint
  PROPERTY INCLUDE_DIRECTORIES
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/gst-plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.5_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(test_uri_endpoint
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boost_LIBRARIES}
)
