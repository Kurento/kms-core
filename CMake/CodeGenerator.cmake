cmake_minimum_required(VERSION 2.8)

find_package(KtoolRomProcessor REQUIRED)

include (GNUInstallDirs)

set (KTOOL_ROM_PROCESSOR_CHECK_FORMAT FALSE CACHE BOOL "Check if codding style of generated code is correct")
mark_as_advanced(KTOOL_ROM_PROCESSOR_CHECK_FORMAT)

set (KURENTO_MODULES_DIR /usr/share/kurento/modules CACHE STRING "Directory where kurento modules descriptors can be found")
mark_as_advanced(KURENTO_MODULES_DIR)

set (KURENTO_MODULES_DIR_INSTALL_PREFIX kurento/modules CACHE STRING "Directory where kurento modules descriptors are installed (relative to \${CMAKE_INSTAL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR})")
mark_as_advanced(KURENTO_MODULES_DIR_INSTALL_PREFIX)

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

  set (OPTION_PARAMS
    NO_OVERWRITE
  )

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

  cmake_parse_arguments("PARAM" "${OPTION_PARAMS}" "${ONE_VALUE_PARAMS}" "${MULTI_VALUE_PARAMS}" ${ARGN})

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

  if (PARAM_NO_OVERWRITE)
    set (COMMAND_LINE ${COMMAND_LINE} -n)
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

  get_values_from_model(PREFIX VALUE MODELS ${PARAM_MODELS} KEYS code.implementation.lib name)
  string (REGEX REPLACE "^lib" "" VALUE_CODE_IMPLEMENTATION_LIB ${VALUE_CODE_IMPLEMENTATION_LIB})
  set (LIBRARY_NAME ${VALUE_CODE_IMPLEMENTATION_LIB} CACHE INTERNAL "Library name got from model")

  set(CUSTOM_PREFIX "kurento")

  set (KTOOL_PROCESSOR_LINE ${KTOOL_ROM_PROCESSOR_EXECUTABLE} -r ${PARAM_MODELS} -dr ${KURENTO_MODULES_DIR})

  ###############################################################
  # Calculate modules dependencies
  ###############################################################

  execute_process(
    COMMAND ${KTOOL_PROCESSOR_LINE} -it cpp_cmake_dependencies -c ${CMAKE_CURRENT_BINARY_DIR} -lf
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

  #Generate source for public interface files
  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${GEN_FILES_DIR}
    INTERNAL_TEMPLATES_DIR cpp_interface
    SOURCE_FILES_OUTPUT INTERFACE_GENERATED_SOURCES
    HEADER_FILES_OUTPUT INTERFACE_GENERATED_HEADERS
  )

  #Generate source for internal interface files
  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${GEN_FILES_DIR}
    INTERNAL_TEMPLATES_DIR cpp_interface_internal
    SOURCE_FILES_OUTPUT INTERFACE_INTERNAL_GENERATED_SOURCES
    HEADER_FILES_OUTPUT INTERFACE_INTERNAL_GENERATED_HEADERS
  )

  #Generate source for internal interface files
  add_library (${VALUE_CODE_IMPLEMENTATION_LIB}interface
    ${PARAM_INTERFACE_LIB_EXTRA_SOURCES}
    ${PARAM_INTERFACE_LIB_EXTRA_HEADERS}
    ${INTERFACE_INTERNAL_GENERATED_SOURCES}
    ${INTERFACE_INTERNAL_GENERATED_HEADERS}
    ${INTERFACE_GENERATED_SOURCES}
    ${INTERFACE_GENERATED_HEADERS}
  )

  target_link_libraries (${VALUE_CODE_IMPLEMENTATION_LIB}interface
    ${DEPENDENCIES_LIBRARIES}
    ${PARAM_INTERFACE_LIB_EXTRA_LIBRARIES}
  )

  set_property (TARGET ${VALUE_CODE_IMPLEMENTATION_LIB}interface
    PROPERTY INCLUDE_DIRECTORIES
      ${DEPENDENCIES_INCLUDE_DIRS}
      ${PARAM_INTERFACE_LIB_EXTRA_INCLUDE_DIRS}
  )

  set(INCLUDE_PREFIX "${CMAKE_INSTALL_INCLUDEDIR}/${CUSTOM_PREFIX}/modules/core")

  set_property (TARGET ${VALUE_CODE_IMPLEMENTATION_LIB}interface
    PROPERTY PUBLIC_HEADER
      ${PARAM_INTERFACE_LIB_EXTRA_HEADERS}
      ${INTERFACE_GENERATED_HEADERS}
  )

  set_target_properties(${VALUE_CODE_IMPLEMENTATION_LIB}interface PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    COMPILE_FLAGS "-fPIC"
  )

  install(TARGETS ${VALUE_CODE_IMPLEMENTATION_LIB}interface
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${INCLUDE_PREFIX}
  )

  ###############################################################
  # Create pkgconfig files
  ###############################################################

  set(prefix ${CMAKE_INSTALL_PREFIX})
  set(exec_prefix "\${prefix}")
  set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
  set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}/${CUSTOM_PREFIX}/modules/${VALUE_NAME}")

  execute_process (
     COMMAND ${KTOOL_PROCESSOR_LINE} -it cpp_pkgconfig -c ${CMAKE_CURRENT_BINARY_DIR} -lf
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

  # Generate internal server files
  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${SERVER_INTERNAL_GEN_FILES_DIR}
    INTERNAL_TEMPLATES_DIR cpp_server_internal
    SOURCE_FILES_OUTPUT SERVER_INTERNAL_GENERATED_SOURCES
    HEADER_FILES_OUTPUT SERVER_INTERNAL_GENERATED_HEADERS
  )

  set(MODULE_GEN_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/implementation/generated-cpp)

  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${MODULE_GEN_FILES_DIR}
    INTERNAL_TEMPLATES_DIR cpp_module
    SOURCE_FILES_OUTPUT MODULE_GENERATED_SOURCES
    HEADER_FILES_OUTPUT MODULE_GENERATED_HEADERS
  )

  set(SERVER_GEN_FILES_DIR ${PARAM_SERVER_STUB_DESTINATION})

  # Generate stub files
  generate_sources (
    MODELS ${PARAM_MODELS}
    GEN_FILES_DIR ${SERVER_GEN_FILES_DIR}
    INTERNAL_TEMPLATES_DIR cpp_server
    SOURCE_FILES_OUTPUT SERVER_GENERATED_SOURCES
    HEADER_FILES_OUTPUT SERVER_GENERATED_HEADERS
    NO_OVERWRITE
  )

  add_library (${VALUE_CODE_IMPLEMENTATION_LIB}impl SHARED
    ${PARAM_SERVER_IMPL_LIB_EXTRA_SOURCES}
    ${PARAM_SERVER_IMPL_LIB_EXTRA_HEADERS}
    ${SERVER_GENERATED_SOURCES}
    ${SERVER_GENERATED_HEADERS}
    ${SERVER_INTERNAL_GENERATED_SOURCES}
    ${SERVER_INTERNAL_GENERATED_HEADERS}
  )

  add_dependencies(${VALUE_CODE_IMPLEMENTATION_LIB}impl
    ${VALUE_CODE_IMPLEMENTATION_LIB}interface
    ${VALUE_CODE_IMPLEMENTATION_LIB}gstreamer
  )

  target_link_libraries (${VALUE_CODE_IMPLEMENTATION_LIB}impl
    ${DEPENDENCIES_LIBRARIES}
    ${PARAM_SERVER_IMPL_LIB_EXTRA_LIBRARIES}
    ${VALUE_CODE_IMPLEMENTATION_LIB}interface
  )

  set (SERVER_PUBLIC_HEADERS
    ${PARAM_SERVER_IMPL_LIB_EXTRA_HEADERS}
    ${SERVER_GENERATED_HEADERS}
  )

  set_property (TARGET ${VALUE_CODE_IMPLEMENTATION_LIB}impl
    PROPERTY PUBLIC_HEADER
      ${PARAM_SERVER_IMPL_LIB_EXTRA_HEADERS}
      ${SERVER_GENERATED_HEADERS}
  )

  set_target_properties(${VALUE_CODE_IMPLEMENTATION_LIB}impl PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
  )

  install(
    TARGETS ${VALUE_CODE_IMPLEMENTATION_LIB}impl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${INCLUDE_PREFIX}
  )

  set (PARAM_SERVER_IMPL_LIB_EXTRA_INCLUDE_DIRS
    ${PARAM_SERVER_IMPL_LIB_EXTRA_INCLUDE_DIRS}
    ${PARAM_INTERFACE_LIB_EXTRA_INCLUDE_DIRS}
  )

  set_property (TARGET ${VALUE_CODE_IMPLEMENTATION_LIB}impl
    PROPERTY INCLUDE_DIRECTORIES
      ${DEPENDENCIES_INCLUDE_DIRS}
      ${PARAM_SERVER_IMPL_LIB_EXTRA_INCLUDE_DIRS}
      ${SERVER_GEN_FILES_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}/interface/generated-cpp
  )

  ###############################################################
  # Server module
  ###############################################################
  add_library (${VALUE_CODE_IMPLEMENTATION_LIB}module MODULE
    ${MODULE_GENERATED_SOURCES}
    ${MODULE_GENERATED_HEADERS}
  )

  add_dependencies(${VALUE_CODE_IMPLEMENTATION_LIB}module
    ${VALUE_CODE_IMPLEMENTATION_LIB}impl
  )

  target_link_libraries (${VALUE_CODE_IMPLEMENTATION_LIB}module
    ${VALUE_CODE_IMPLEMENTATION_LIB}impl
    ${VALUE_CODE_IMPLEMENTATION_LIB}interface
  )

  set_property (TARGET ${VALUE_CODE_IMPLEMENTATION_LIB}module
    PROPERTY INCLUDE_DIRECTORIES
      ${DEPENDENCIES_INCLUDE_DIRS}
      ${CMAKE_CURRENT_SOURCE_DIR}/implementation
      ${SERVER_GEN_FILES_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}/interface/generated-cpp
  )

  install(
    TARGETS ${VALUE_CODE_IMPLEMENTATION_LIB}module
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CUSTOM_PREFIX}/modules
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CUSTOM_PREFIX}/modules
  )

  execute_process(
    COMMAND ${KTOOL_ROM_PROCESSOR_EXECUTABLE} -r ${PARAM_MODELS} -dr ${KURENTO_MODULES_DIR} -o ${CMAKE_CURRENT_BINARY_DIR}/kmd
    OUTPUT_VARIABLE PROCESSOR_OUTPUT
  )

  message (Generating final rom model\n
    ${PROCESSOR_OUTPUT}
  )

  file (GLOB_RECURSE FINAL_MODELS ${CMAKE_CURRENT_BINARY_DIR}/kmd/*kmd.json)

  install(FILES
    ${FINAL_MODELS}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/${KURENTO_MODULES_DIR_INSTALL_PREFIX}
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
