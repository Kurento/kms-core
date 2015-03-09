set (SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp")
set (VALGRING_TEST_PROPERTIES
  DEBUG_MEDIASET=1
)

set (TEST_PROPERTIES
  GST_PLUGIN_PATH=${CMAKE_BINARY_DIR}
  GST_DEBUG_DUMP_DOT_DIR=${KURENTO_DOT_DIR}
  )

add_test_program (test_module_manager moduleManager.cpp)
add_dependencies(test_module_manager ${LIBRARY_NAME}module ${LIBRARY_NAME}impl)
set_property (TARGET test_module_manager
  PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.0_INCLUDE_DIRS}
)
target_link_libraries(test_module_manager
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
)

add_test_program (test_media_element mediaElement.cpp)
add_dependencies(test_media_element kmscoreplugins ${LIBRARY_NAME}impl)
set_property (TARGET test_media_element
  PROPERTY INCLUDE_DIRECTORIES
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.0_INCLUDE_DIRS}
    ${Boot_INCLUDE_DIRS}
)
target_link_libraries(test_media_element
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boot_LIBRARIES}
)

add_test_program (test_sdp_endpoint sdpEndpoint.cpp)
add_dependencies(test_sdp_endpoint kmscoreplugins ${LIBRARY_NAME}impl)
set_property (TARGET test_sdp_endpoint
  PROPERTY INCLUDE_DIRECTORIES
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.0_INCLUDE_DIRS}
    ${Boot_INCLUDE_DIRS}
)
target_link_libraries(test_sdp_endpoint
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boot_LIBRARIES}
)

add_test_program (test_media_object mediaObject.cpp)
add_dependencies(test_media_object kmscoreplugins ${LIBRARY_NAME}impl)
set_property (TARGET test_media_object
  PROPERTY INCLUDE_DIRECTORIES
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.0_INCLUDE_DIRS}
    ${Boot_INCLUDE_DIRS}
)
target_link_libraries(test_media_object
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boot_LIBRARIES}
)

add_test_program (test_complex_type complexType.cpp)
add_dependencies(test_complex_type kmscoreplugins ${LIBRARY_NAME}impl)
set_property (TARGET test_complex_type
  PROPERTY INCLUDE_DIRECTORIES
    ${KmsJsonRpc_INCLUDE_DIRS}
    ${sigc++-2.0_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${glibmm-2.4_INCLUDE_DIRS}
    ${gstreamer-1.0_INCLUDE_DIRS}
    ${Boot_INCLUDE_DIRS}
)
target_link_libraries(test_complex_type
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
  ${Boot_LIBRARIES}
)
