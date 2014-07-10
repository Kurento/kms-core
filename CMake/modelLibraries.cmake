cmake_minimum_required(VERSION 2.8)

include (CMakeParseArguments)
include (CodeGenerator)

function (generate_kurento_libraries)
  set (OPTION_PARAMS
  )

  set (ONE_VALUE_PARAMS
    SERVER_STUB_DESTINATION
  )

  set (MULTI_VALUE_PARAMS
    MODELS
    INTERFACE_LIB_EXTRA_SOURCES
    INTERFACE_LIB_EXTRA_HEADERS
    INTERFACE_LIB_EXTRA_INCLUDE_DIRS
    INTERFACE_LIB_EXTRA_LIBRARIES
    SERVER_IMPL_LIB_EXTRA_SOURCES
    SERVER_IMPL_LIB_EXTRA_HEADERS
    SERVER_IMPL_LIB_EXTRA_INCLUDE_DIRS
    SERVER_IMPL_LIB_EXTRA_LIBRARIES
  )

  set (REQUIRED_PARAMS
    MODELS
    SERVER_STUB_DESTINATION
  )

  cmake_parse_arguments("PARAM" "${OPTION_PARAMS}" "${ONE_VALUE_PARAMS}" "${MULTI_VALUE_PARAMS}" ${ARGN})

  foreach (REQUIRED_PARAM ${REQUIRED_PARAMS})
    if (NOT DEFINED PARAM_${REQUIRED_PARAM})
      message (FATAL_ERROR "Required param ${REQUIRED_PARAM} is not set")
    endif()
  endforeach()

  find_package(PkgConfig)

  ###############################################################
  # Dependencies
  ###############################################################

  set (GST_REQUIRED 1.3.3)

  set (JSONRPC_REQUIRED 0.0.6)
  set (SIGCPP_REQUIRED 2.0.10)
  set (GLIBMM_REQUIRED 2.37)

  pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=${GST_REQUIRED})

  pkg_check_modules(JSONRPC REQUIRED libjsonrpc>=${JSONRPC_REQUIRED})
  pkg_check_modules(SIGCPP REQUIRED sigc++-2.0>=${SIGCPP_REQUIRED})
  pkg_check_modules(GLIBMM REQUIRED glibmm-2.4>=${GLIBMM_REQUIRED})

  ###############################################################
  # Interface library
  ###############################################################

  set(GEN_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/interface/generated-cpp)

  set(INTERFACE_TEMPLATES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/interface/templates/cpp_interface)

  #Generate source for public interface files
  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${GEN_FILES_DIR}
    TEMPLATES_DIR ${INTERFACE_TEMPLATES_DIR}
    SOURCE_FILES_OUTPUT INTERFACE_GENERATED_SOURCES
    HEADER_FILES_OUTPUT INTERFACE_GENERATED_HEADERS
  )

  set(INTERFACE_INTERNAL_TEMPLATES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/interface/templates/cpp_interface_internal)

  #Generate source for internal interface files
  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${GEN_FILES_DIR}
    TEMPLATES_DIR ${INTERFACE_INTERNAL_TEMPLATES_DIR}
    SOURCE_FILES_OUTPUT INTERFACE_INTERNAL_GENERATED_SOURCES
    HEADER_FILES_OUTPUT INTERFACE_INTERNAL_GENERATED_HEADERS
  )

  #Generate source for internal interface files
  add_library (kms-core-interface
    ${PARAM_INTERFACE_LIB_EXTRA_SOURCES}
    ${PARAM_INTERFACE_LIB_EXTRA_HEADERS}
    ${INTERFACE_INTERNAL_GENERATED_SOURCES}
    ${INTERFACE_INTERNAL_GENERATED_HEADERS}
    ${INTERFACE_GENERATED_SOURCES}
    ${INTERFACE_GENERATED_HEADERS}
  )

  target_link_libraries (kms-core-interface
    ${JSONRPC_LIBRARIES}
    ${SIGCPP_LIBRARIES}
    ${PARAM_INTERFACE_LIB_EXTRA_LIBRARIES}
  )

  set_property (TARGET kms-core-interface
    PROPERTY INCLUDE_DIRECTORIES
      ${JSONRPC_INCLUDE_DIRS}
      ${SIGCPP_INCLUDE_DIRS}
      ${PARAM_INTERFACE_LIB_EXTRA_INCLUDE_DIRS}
  )

  set(CUSTOM_PREFIX "kurento")
  set(INCLUDE_PREFIX "${CMAKE_INSTALL_INCLUDEDIR}/${CUSTOM_PREFIX}/modules/core")

  set_property (TARGET kms-core-interface
    PROPERTY PUBLIC_HEADER
      ${PARAM_INTERFACE_LIB_EXTRA_HEADERS}
      ${INTERFACE_GENERATED_HEADERS}
  )

  set_target_properties(kms-core-interface PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    COMPILE_FLAGS "-fPIC"
  )

  install(TARGETS kms-core-interface
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${INCLUDE_PREFIX}
  )

  ###############################################################
  # Server implementation library
  ###############################################################

  set(SERVER_INTERNAL_GEN_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/implementation/generated-cpp)
  set(SERVER_INTERNAL_TEMPLATES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/implementation/templates/cpp_server_internal)

  # Generate internal server files
  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${SERVER_INTERNAL_GEN_FILES_DIR}
    TEMPLATES_DIR ${SERVER_INTERNAL_TEMPLATES_DIR}
    SOURCE_FILES_OUTPUT SERVER_INTERNAL_GENERATED_SOURCES
    HEADER_FILES_OUTPUT SERVER_INTERNAL_GENERATED_HEADERS
  )

  set(MODULE_GEN_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/implementation/generated-cpp)
  set(MODULE_TEMPLATES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/implementation/templates/cpp_module)

  # Generate stub files
  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${MODULE_GEN_FILES_DIR}
    TEMPLATES_DIR ${MODULE_TEMPLATES_DIR}
    SOURCE_FILES_OUTPUT MODULE_GENERATED_SOURCES
    HEADER_FILES_OUTPUT MODULE_GENERATED_HEADERS
  )

  set(SERVER_GEN_FILES_DIR ${PARAM_SERVER_STUB_DESTINATION})
  set(SERVER_TEMPLATES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/implementation/templates/cpp_server)

  # Generate public server files
  # TODO: Add an option to not delete the code if already exists
  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${SERVER_GEN_FILES_DIR}
    TEMPLATES_DIR ${SERVER_TEMPLATES_DIR}
    SOURCE_FILES_OUTPUT SERVER_GENERATED_SOURCES
    HEADER_FILES_OUTPUT SERVER_GENERATED_HEADERS
  )

  add_library (kms-core-impl SHARED
    ${PARAM_SERVER_IMPL_LIB_EXTRA_SOURCES}
    ${PARAM_SERVER_IMPL_LIB_EXTRA_HEADERS}
    ${SERVER_GENERATED_SOURCES}
    ${SERVER_GENERATED_HEADERS}
    ${SERVER_INTERNAL_GENERATED_SOURCES}
    ${SERVER_INTERNAL_GENERATED_HEADERS}
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
    ${PARAM_SERVER_IMPL_LIB_EXTRA_LIBRARIES}
    kms-core-interface
  )

  set (SERVER_PUBLIC_HEADERS
    ${PARAM_SERVER_IMPL_LIB_EXTRA_HEADERS}
    ${SERVER_GENERATED_HEADERS}
  )

  set_property (TARGET kms-core-impl
    PROPERTY PUBLIC_HEADER
      ${PARAM_SERVER_IMPL_LIB_EXTRA_HEADERS}
      ${SERVER_GENERATED_HEADERS}
  )

  set_target_properties(kms-core-impl PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
  )

  install(
    TARGETS kms-core-impl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${INCLUDE_PREFIX}
  )

  set (PARAM_SERVER_IMPL_LIB_EXTRA_INCLUDE_DIRS
    ${PARAM_SERVER_IMPL_LIB_EXTRA_INCLUDE_DIRS}
    ${PARAM_INTERFACE_LIB_EXTRA_INCLUDE_DIRS}
  )

  set_property (TARGET kms-core-impl
    PROPERTY INCLUDE_DIRECTORIES
      ${GLIBMM_INCLUDE_DIRS}
      ${JSONRPC_INCLUDE_DIRS}
      ${SIGCPP_INCLUDE_DIRS}
      ${GSTREAMER_INCLUDE_DIRS}
      ${PARAM_SERVER_IMPL_LIB_EXTRA_INCLUDE_DIRS}
      ${SERVER_GEN_FILES_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}/interface/generated-cpp
  )

  ###############################################################
  # Server module
  ###############################################################
  add_library (kms-core-module MODULE
    ${MODULE_GENERATED_SOURCES}
    ${MODULE_GENERATED_HEADERS}
  )

  add_dependencies(kms-core-module
    kms-core-impl
  )

  target_link_libraries (kms-core-module
    kms-core-impl
    kms-core-interface
  )

  set_property (TARGET kms-core-module
    PROPERTY INCLUDE_DIRECTORIES
      ${JSONRPC_INCLUDE_DIRS}
      ${SIGCPP_INCLUDE_DIRS}
      ${CMAKE_CURRENT_SOURCE_DIR}/implementation
      ${SERVER_GEN_FILES_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}/interface/generated-cpp
  )

  install(
    TARGETS kms-core-module
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CUSTOM_PREFIX}/modules
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CUSTOM_PREFIX}/modules
  )

endfunction()
