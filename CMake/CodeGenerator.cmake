cmake_minimum_required(VERSION 2.8)

find_package(KtoolRomProcessor REQUIRED)

set (KTOOL_ROM_PROCESSOR_CHECK_FORMAT FALSE CACHE BOOL "Check if codding style of generated code is correct")
mark_as_advanced(KTOOL_ROM_PROCESSOR_CHECK_FORMAT)

set (KURENTO_MODULES_DIR /usr/share/kurento/modules CACHE STRING "Directory where kurento modules are installed")
mark_as_advanced(KURENTO_MODULES_DIR)

include (CMakeParseArguments)

set (PROCESSED_PREFIX "Processed file:\t")

# generate_sources (
#  MODELS list of models or directories
#  GEN_FILES_DIR directory to generate files
#  SOURCE_FILES_OUTPUT variable to output generated sources
#  HEADER_FILES_OUTPUT variable to output generated headers
#  [TEMPLATES_DIR] templates directory
#  [INTERNAL_TEMPLATES_DIR] internal templates directory,
#                       only used if TEMPLATES_DIR is not set
#
#)
function (generate_sources)
  set (ONE_VALUE_PARAMS
    GEN_FILES_DIR
    SOURCE_FILES_OUTPUT
    HEADER_FILES_OUTPUT
    TEMPLATES_DIR
    INTERNAL_TEMPLATES_DIR
  )
  set (MULTI_VALUE_PARAMS
    MODELS
  )

  set (REQUIRED_PARAMS
    MODELS
    GEN_FILES_DIR
    SOURCE_FILES_OUTPUT
    HEADER_FILES_OUTPUT
  )

  cmake_parse_arguments("PARAM" "" "${ONE_VALUE_PARAMS}" "${MULTI_VALUE_PARAMS}" ${ARGN})

  foreach (REQUIRED_PARAM ${REQUIRED_PARAMS})
    if (NOT DEFINED PARAM_${REQUIRED_PARAM})
      message (FATAL_ERROR "Required param ${REQUIRED_PARAM} is not set")
    endif()
  endforeach()

  set (COMMAND_LINE -c ${PARAM_GEN_FILES_DIR} -r ${PARAM_MODELS} -dr ${KURENTO_MODULES_DIR} -lf)

  if (DEFINED PARAM_TEMPLATES_DIR)
    set (COMMAND_LINE ${COMMAND_LINE} -t ${PARAM_TEMPLATES_DIR})
  elseif (DEFINED PARAM_INTERNAL_TEMPLATES_DIR)
    set (COMMAND_LINE ${COMMAND_LINE} -it ${PARAM_INTERNAL_TEMPLATES_DIR})
  else()
    message (FATAL_ERROR "Missing templates you have to set TEMPLATES_DIR INTERNAL_TEMPLATES_DIR")
  endif()

  foreach (MODEL ${PARAM_MODELS})
    if (IS_DIRECTORY ${MODEL})
      file (GLOB_RECURSE MODELS ${MODEL}/*kmd.json)
      list (APPEND MODEL_FILES ${MODELS})
    elseif (EXISTS ${MODEL})
      list (APPEND MODEL_FILES ${MODEL})
    endif()
  endforeach()

  if (NOT DEFINED MODEL_FILES)
    message (FATAL_ERROR "No model files found")
  endif ()

  message (STATUS "Running code generator")
  execute_process(
    COMMAND ${KTOOL_ROM_PROCESSOR_EXECUTABLE} ${COMMAND_LINE}
    OUTPUT_VARIABLE PROCESSOR_OUTPUT
  )

  if ("${PROCESSOR_OUTPUT}" STREQUAL "")
    message (FATAL_ERROR "No code generated")
  else()
    string(REPLACE "\n" ";" PROCESSOR_OUTPUT ${PROCESSOR_OUTPUT})
  endif()

  foreach (_FILE ${PROCESSOR_OUTPUT})
    if (${_FILE} MATCHES "${PROCESSED_PREFIX}.*")
      string(REPLACE ${PROCESSED_PREFIX} "" _FILE ${_FILE})
      string(REGEX REPLACE "\t.*" "" _FILE ${_FILE})
      if (${_FILE} MATCHES ".*cpp")
        list (APPEND GENERATED_SOURCE_FILES ${PARAM_GEN_FILES_DIR}/${_FILE})
      elseif(${_FILE} MATCHES ".*hpp")
        list (APPEND GENERATED_HEADER_FILES ${PARAM_GEN_FILES_DIR}/${_FILE})
      endif ()
      if (KTOOL_ROM_PROCESSOR_CHECK_FORMAT)
        if (EXISTS ${PARAM_GEN_FILES_DIR}/${_FILE}.orig)
          execute_process (COMMAND rm ${PARAM_GEN_FILES_DIR}/${_FILE}.orig)
        endif()
        execute_process (
          COMMAND astyle
            --style=linux
            --indent=spaces=2
            --indent-preprocessor
            --min-conditional-indent=2
            --break-blocks
            --pad-oper
            --pad-paren-out
            --convert-tabs
            --align-pointer=name
            --lineend=linux
            --break-blocks
            -q
            ${PARAM_GEN_FILES_DIR}/${_FILE}
        )
        if (EXISTS ${PARAM_GEN_FILES_DIR}/${_FILE}.orig)
          message (WARNING "Style incorrect for file: ${PARAM_GEN_FILES_DIR}/${_FILE}")
        endif()
      endif()
    else()
      message (" Generator -> ${_FILE}")
    endif()
  endforeach()

  set (${PARAM_SOURCE_FILES_OUTPUT} ${${PARAM_SOURCE_FILES_OUTPUT}} ${GENERATED_SOURCE_FILES} PARENT_SCOPE)
  set (${PARAM_HEADER_FILES_OUTPUT} ${${PARAM_HEADER_FILES_OUTPUT}} ${GENERATED_HEADER_FILES} PARENT_SCOPE)

endfunction()

function (generate_code)
  set (OPTION_PARAMS
  )

  set (ONE_VALUE_PARAMS
  )
  set (MULTI_VALUE_PARAMS
    MODELS
  )

  set (REQUIRED_PARAMS
    MODELS
  )

  cmake_parse_arguments("PARAM" "${OPTION_PARAMS}" "${ONE_VALUE_PARAMS}" "${MULTI_VALUE_PARAMS}" ${ARGN})

  foreach (REQUIRED_PARAM ${REQUIRED_PARAMS})
    if (NOT DEFINED PARAM_${REQUIRED_PARAM})
      message (FATAL_ERROR "Required param ${REQUIRED_PARAM} is not set")
    endif()
  endforeach()

  foreach (MODEL ${PARAM_MODELS})
    if (IS_DIRECTORY ${MODEL})
      file (GLOB_RECURSE MODELS ${MODEL}/*kmd.json)
      list (APPEND MODEL_FILES ${MODELS})
    elseif (EXISTS ${MODEL})
      list (APPEND MODEL_FILES ${MODEL})
    endif()
  endforeach()

  if (NOT DEFINED MODEL_FILES)
    message (FATAL_ERROR "No model files found")
  endif ()

  add_custom_target(generate_code
    touch generate_code
  )

  add_custom_command (
    COMMENT               "Regenerating source from: ${MODEL_FILE}"
    TARGET                generate_code
    DEPENDS               ${MODEL_FILES}
    COMMAND               ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY     ${CMAKE_BINARY_DIR}
  )

  generate_kurento_libraries (${ARGN})

endfunction()

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

  set(CUSTOM_PREFIX "kurento")

  set (KURENTO_MODULES_DIR /usr/share/kurento/modules)
  set (KTOOL_PROCESSOR_LINE ${KTOOL_ROM_PROCESSOR_EXECUTABLE} -r ${PARAM_MODELS} -dr ${KURENTO_MODULES_DIR})

  ###############################################################
  # Calculate modules dependencies
  ###############################################################

  execute_process(
    COMMAND ${KTOOL_PROCESSOR_LINE} -t ${CMAKE_CURRENT_SOURCE_DIR}/templates/ -c ${CMAKE_CURRENT_BINARY_DIR}
    OUTPUT_VARIABLE PROCESSOR_OUTPUT
  )

  if ("${PROCESSOR_OUTPUT}" STREQUAL "")
    message (FATAL_ERROR "No cmake dependencies generated")
  else()
    string(REPLACE "\n" ";" PROCESSOR_OUTPUT ${PROCESSOR_OUTPUT})
  endif()

  foreach (_FILE ${PROCESSOR_OUTPUT})
    if (${_FILE} MATCHES "${PROCESSED_PREFIX}.*")
      string(REPLACE ${PROCESSED_PREFIX} "" _FILE ${_FILE})
      string(REGEX REPLACE "\t.*" "" _FILE ${_FILE})
      if (${_FILE} MATCHES ".*cmake")
        include (${CMAKE_CURRENT_BINARY_DIR}/${_FILE})
      else()
        message (WARNING "Unexpected file generated ${_FILE}")
      endif ()
    else()
      message (" Generator -> ${_FILE}")
    endif()
  endforeach()

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
  add_library (kms${PARAM_MODDULE_NAME}interface
    ${PARAM_INTERFACE_LIB_EXTRA_SOURCES}
    ${PARAM_INTERFACE_LIB_EXTRA_HEADERS}
    ${INTERFACE_INTERNAL_GENERATED_SOURCES}
    ${INTERFACE_INTERNAL_GENERATED_HEADERS}
    ${INTERFACE_GENERATED_SOURCES}
    ${INTERFACE_GENERATED_HEADERS}
  )

  target_link_libraries (kms${PARAM_MODDULE_NAME}interface
    ${JSONRPC_LIBRARIES}
    ${SIGCPP_LIBRARIES}
    ${PARAM_INTERFACE_LIB_EXTRA_LIBRARIES}
  )

  set_property (TARGET kms${PARAM_MODDULE_NAME}interface
    PROPERTY INCLUDE_DIRECTORIES
      ${JSONRPC_INCLUDE_DIRS}
      ${SIGCPP_INCLUDE_DIRS}
      ${PARAM_INTERFACE_LIB_EXTRA_INCLUDE_DIRS}
  )

  set(INCLUDE_PREFIX "${CMAKE_INSTALL_INCLUDEDIR}/${CUSTOM_PREFIX}/modules/core")

  set_property (TARGET kms${PARAM_MODDULE_NAME}interface
    PROPERTY PUBLIC_HEADER
      ${PARAM_INTERFACE_LIB_EXTRA_HEADERS}
      ${INTERFACE_GENERATED_HEADERS}
  )

  set_target_properties(kms${PARAM_MODDULE_NAME}interface PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    COMPILE_FLAGS "-fPIC"
  )

  install(TARGETS kms${PARAM_MODDULE_NAME}interface
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${INCLUDE_PREFIX}
  )

  ###############################################################
  # Create pkgconfig files
  ###############################################################

  include (GNUInstallDirs)
  set(prefix ${CMAKE_INSTALL_PREFIX})
  set(exec_prefix "\${prefix}")
  set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
  set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}/${CUSTOM_PREFIX}")

  execute_process (
     COMMAND ${KTOOL_PROCESSOR_LINE} -t ${CMAKE_CURRENT_SOURCE_DIR}/pkg-config-templates/ -c ${CMAKE_CURRENT_BINARY_DIR} -lf
     OUTPUT_VARIABLE PROCESSOR_OUTPUT
  )

  if ("${PROCESSOR_OUTPUT}" STREQUAL "")
    message (FATAL_ERROR "No cmake pkg-config files generated")
  else()
    string(REPLACE "\n" ";" PROCESSOR_OUTPUT ${PROCESSOR_OUTPUT})
  endif()

  foreach (_FILE ${PROCESSOR_OUTPUT})
    if (${_FILE} MATCHES "${PROCESSED_PREFIX}.*")
      string(REPLACE ${PROCESSED_PREFIX} "" _FILE ${_FILE})
      string(REGEX REPLACE "\t.*" "" _FILE ${_FILE})
      string(REPLACE ".pc.in" ".pc" _OUT_FILE ${_FILE})
      if (${_FILE} MATCHES ".*pc.in")
        configure_file(${CMAKE_CURRENT_BINARY_DIR}/${_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${_OUT_FILE} @ONLY)
        install(FILES
          ${CMAKE_CURRENT_BINARY_DIR}/${_OUT_FILE}
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        )
      else()
        message (WARNING "Unexpected file generated ${_FILE}")
      endif ()
    else()
      message (" Generator -> ${_FILE}")
    endif()
  endforeach()

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
    kms${PARAM_MODDULE_NAME}interface
    kmscore
  )

  target_link_libraries (kms-core-impl
    ${GLIBMM_LIBRARIES}
    ${JSONRPC_LIBRARIES}
    ${SIGCPP_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${PARAM_SERVER_IMPL_LIB_EXTRA_LIBRARIES}
    kms${PARAM_MODDULE_NAME}interface
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
    kms${PARAM_MODDULE_NAME}interface
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

function (get_values_from_model)
  set (OPTION_PARAMS
  )

  set (ONE_VALUE_PARAMS
    PREFIX
  )

  set (MULTI_VALUE_PARAMS
    MODELS
    KEYS
  )

  set (REQUIRED_PARAMS
    MODELS
    KEYS
  )

  cmake_parse_arguments("PARAM" "${OPTION_PARAMS}" "${ONE_VALUE_PARAMS}" "${MULTI_VALUE_PARAMS}" ${ARGN})

  foreach (REQUIRED_PARAM ${REQUIRED_PARAMS})
    if (NOT DEFINED PARAM_${REQUIRED_PARAM})
      message (FATAL_ERROR "Required param ${REQUIRED_PARAM} is not set")
    endif()
  endforeach()

  execute_process(
    COMMAND ${KTOOL_ROM_PROCESSOR_EXECUTABLE} -r ${PARAM_MODELS} -dr ${KURENTO_MODULES_DIR} -s ${PARAM_KEYS}
    OUTPUT_VARIABLE PROCESSOR_OUTPUT
  )

  if ("${PROCESSOR_OUTPUT}" STREQUAL "")
    message (FATAL_ERROR "No values found")
  else()
    string(REPLACE "\n" ";" PROCESSOR_OUTPUT ${PROCESSOR_OUTPUT})
  endif()

  set (VALUE_PREFIX "Value: ")

  foreach (_LINE ${PROCESSOR_OUTPUT})
    if (${_LINE} MATCHES "${VALUE_PREFIX}.*")
      string(REPLACE ${VALUE_PREFIX} "" _LINE ${_LINE})
      message ("Got ${_LINE}")
      foreach (_KEY ${PARAM_KEYS})
          if (${_LINE} MATCHES "${_KEY} = ")
            string(REPLACE "${_KEY} = " "" _LINE ${_LINE})
            string(REPLACE "." "_" _KEY ${_KEY})
            string(TOUPPER ${_KEY} _KEY)
            if (DEFINED PARAM_PREFIX)
              set (${PARAM_PREFIX}_${_KEY} ${_LINE} PARENT_SCOPE)
            else()
              set (${_KEY} ${_LINE} PARENT_SCOPE)
            endif()
          endif()
      endforeach ()
    else()
      message (" Generator -> ${_LINE}")
    endif()
  endforeach()

endfunction ()
