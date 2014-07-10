cmake_minimum_required(VERSION 2.8)

find_package(KtoolRomProcessor REQUIRED)

set (KTOOL_ROM_PROCESSOR_CHECK_FORMAT FALSE CACHE BOOL "Check if codding style of generated code is correct")
mark_as_advanced(KTOOL_ROM_PROCESSOR_CHECK_FORMAT)

include (CMakeParseArguments)

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

  set (COMMAND_LINE -c ${PARAM_GEN_FILES_DIR} -r ${PARAM_MODELS} -lf)

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

  set (PROCESSED_PREFIX "Processed file:\t")

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

  include(${CMAKE_SOURCE_DIR}/CMake/modelLibraries.cmake)
  generate_kurento_libraries (${ARGN})

endfunction()
