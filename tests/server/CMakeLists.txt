add_test_program (test_module module.cpp)
add_dependencies(test_module ${LIBRARY_NAME}module ${LIBRARY_NAME}impl)
set_property (TARGET test_module
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
)
target_link_libraries(test_module
  ${LIBRARY_NAME}impl
  ${glibmm-2.4_LIBRARIES}
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
