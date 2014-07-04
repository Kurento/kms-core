cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

include (CodeGenerator)

set(MODEL ${CMAKE_CURRENT_SOURCE_DIR}/../interface/core.kmd.json)

set(COMMON_GEN_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated-cpp)
set(COMMON_TEMPLATES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/templates/cpp)

generate_sources (${MODEL} ${COMMON_GEN_FILES_DIR} ${COMMON_TEMPLATES_DIR} GENERATED_SOURCES GENERATED_HEADERS)

set(MODULE_GEN_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated-cpp)
set(MODULE_TEMPLATES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/templates/cpp_module)

generate_sources (${MODEL} ${MODULE_GEN_FILES_DIR} ${MODULE_TEMPLATES_DIR} MODULE_GENERATED_SOURCES MODULE_GENERATED_HEADERS)

set(STUB_GEN_FILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated-cpp)
set(STUB_TEMPLATES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/templates/cpp_stub)

# TODO: Add an option to not delete the code if already exists
generate_sources (${MODEL} ${STUB_GEN_FILES_DIR} ${STUB_TEMPLATES_DIR} GENERATED_SOURCES GENERATED_HEADERS)

set (KMS_CORE_IMPL_SOURCES
  EventHandler.cpp
  Factory.cpp
  MediaSet.cpp
)
set (KMS_CORE_IMPL_HEADERS
  EventHandler.hpp
  Factory.hpp
  MediaSet.hpp
  FactoryRegistrar.hpp
)

add_library (kms-core-impl SHARED
  ${KMS_CORE_IMPL_SOURCES}
  ${KMS_CORE_IMPL_HEADERS}
  ${GENERATED_SOURCES}
  ${GENERATED_HEADERS}
)

add_dependencies(kms-core-impl
  kms-core-interface
  kmscore
)

target_link_libraries (kms-core-impl
  ${GLIBMM_LIBRARIES}
  ${JSONRPC_LIBRARIES}
  ${SIGCPP_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
  kms-core-interface
)

set_property (TARGET kms-core-impl
  PROPERTY INCLUDE_DIRECTORIES
    ${GLIBMM_INCLUDE_DIRS}
    ${JSONRPC_INCLUDE_DIRS}
    ${SIGCPP_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STUB_GEN_FILES_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface
    ${CMAKE_CURRENT_BINARY_DIR}/../interface/generated-cpp
)

add_library (kms-core-module MODULE
  ${MODULE_GENERATED_SOURCES}
  ${MODULE_GENERATED_HEADERS}
)

add_dependencies(kms-core-module
  kms-core-impl
)

target_link_libraries (kms-core-module
#   ${GLIBMM_LIBS}
#   ${JSONRPC_LIBS}
#   ${SIGCPP_LIBS}
#   ${GSTREAMER_LIBS}
  kms-core-impl
  kms-core-interface
)

set_property (TARGET kms-core-module
  PROPERTY INCLUDE_DIRECTORIES
    ${JSONRPC_INCLUDE_DIRS}
    ${SIGCPP_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${STUB_GEN_FILES_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/../interface/generated-cpp
)
