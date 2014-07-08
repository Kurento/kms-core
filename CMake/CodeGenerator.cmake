cmake_minimum_required(VERSION 2.8)

find_package(KtoolRomProcessor REQUIRED)

set (KTOOL_ROM_PROCESSOR_CHECK_FORMAT FALSE CACHE BOOL "Check if codding style of generated code is correct")
mark_as_advanced(KTOOL_ROM_PROCESSOR_CHECK_FORMAT)

function (generate_sources MODEL_FILE GEN_FILES_DIR TEMPLATES_DIR SOURCE_FILES_OUTPUT HEADER_FILES_OUTPUT)
  message (STATUS "Running code generator")
  execute_process(
    COMMAND
    ${KTOOL_ROM_PROCESSOR_EXECUTABLE} -c "${GEN_FILES_DIR}" -r ${MODEL_FILE} -t ${TEMPLATES_DIR} -dr ${MODEL_FILE} -lf
    OUTPUT_VARIABLE PROCESSOR_OUTPUT
  )

  string(REPLACE "\n" ";" PROCESSOR_OUTPUT ${PROCESSOR_OUTPUT})
  set (PROCESSED_PREFIX "Processed file:\t")

  foreach (_FILE ${PROCESSOR_OUTPUT})
    if (${_FILE} MATCHES "${PROCESSED_PREFIX}.*")
      string(REPLACE ${PROCESSED_PREFIX} "" _FILE ${_FILE})
      string(REGEX REPLACE "\t.*" "" _FILE ${_FILE})
      if (${_FILE} MATCHES ".*cpp")
        list (APPEND GENERATED_SOURCE_FILES ${GEN_FILES_DIR}/${_FILE})
      elseif(${_FILE} MATCHES ".*hpp")
        list (APPEND GENERATED_HEADER_FILES ${GEN_FILES_DIR}/${_FILE})
      endif ()
      if (KTOOL_ROM_PROCESSOR_CHECK_FORMAT)
        if (EXISTS ${GEN_FILES_DIR}/${_FILE}.orig)
          execute_process (COMMAND rm ${GEN_FILES_DIR}/${_FILE}.orig)
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
            ${GEN_FILES_DIR}/${_FILE}
        )
        if (EXISTS ${GEN_FILES_DIR}/${_FILE}.orig)
          message (WARNING "Style incorrect for file: ${GEN_FILES_DIR}/${_FILE}")
        endif()
      endif()
    else()
      message (" Generator -> ${_FILE}")
    endif()
  endforeach()

  set (${SOURCE_FILES_OUTPUT} ${${SOURCE_FILES_OUTPUT}} ${GENERATED_SOURCE_FILES} PARENT_SCOPE)
  set (${HEADER_FILES_OUTPUT} ${${HEADER_FILES_OUTPUT}} ${GENERATED_HEADER_FILES} PARENT_SCOPE)

  file (GLOB TEMPLATES ${TEMPLATES_DIR}/*ftl)

  add_custom_command (
    COMMENT               "Regenerating source from: ${MODEL_FILE}"
    OUTPUT                ${GENERATED_SOURCE_FILES} ${GENERATED_HEADER_FILES}
    DEPENDS               ${MODEL_FILE} ${TEMPLATES}
    COMMAND               ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY     ${CMAKE_BINARY_DIR}
  )

endfunction()
