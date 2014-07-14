add_test_program (test_module module.cpp)
add_dependencies(test_module ${LIBRARY_NAME}module ${LIBRARY_NAME}impl)
set_property (TARGET test_module
  PROPERTY INCLUDE_DIRECTORIES
    ${JSONRPC_INCLUDE_DIRS}
    ${SIGCPP_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation/generated-cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/server/interface
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/interface/generated-cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../../src/server/implementation/generated-cpp
    ${GLIBMM_INCLUDE_DIRS}
)
target_link_libraries(test_module
  ${LIBRARY_NAME}impl
  ${GLIBMM_LIBRARIES}
)
