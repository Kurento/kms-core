cmake_minimum_required(VERSION 2.8)

set (PROJECT_NAME "gst-kurento-plugins")

project (${PROJECT_NAME})

find_program(GIT_EXEC git)

SET (TAG "${PROJECT_NAME}-")

execute_process(COMMAND ${GIT_EXEC} rev-parse --git-dir OUTPUT_VARIABLE GIT_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)

if(EXISTS ${GIT_EXEC} AND EXISTS "${GIT_DIR}")
  execute_process(COMMAND ${GIT_EXEC} submodule init WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  execute_process(COMMAND ${GIT_EXEC} submodule update WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  execute_process(COMMAND ${GIT_EXEC} remote update WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  execute_process(COMMAND ${GIT_EXEC} describe --abbrev=6 --tags --dirty --always --match "${TAG}*"
                  OUTPUT_VARIABLE PROJECT_VERSION
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  string(REGEX REPLACE "^${TAG}(.*)" "\\1" PROJECT_VERSION ${PROJECT_VERSION})
  if (NOT EXISTS ${GIT_DIR}/hooks/pre-commit)
    execute_process(COMMAND ln -s ${CMAKE_CURRENT_SOURCE_DIR}/hooks/pre-commit.hook
                    "${GIT_DIR}/hooks/pre-commit")
  endif ()
else()
  set(PROJECT_MAJOR_VERSION 1)
  set(PROJECT_MINOR_VERSION 2)
  set(PROJECT_PATCH_VERSION 12-dev)
  set(PROJECT_VERSION
      ${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_VERSION})
endif()

find_package(PkgConfig)

set (GST_REQUIRED 1.2.0)
set (GLIB_REQUIRED 2.38)
set (GNUTLS_REQUIRED 3.2.0)
set (SOUP_REQUIRED 2.40)
set (NICE_REQUIRED 0.1.4)
set (OPENCV_REQUIRED 2.0.0)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-base-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_CHECK REQUIRED gstreamer-check-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_SDP REQUIRED gstreamer-sdp-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_PBUTILS REQUIRED gstreamer-pbutils-1.0>=${GST_REQUIRED})
pkg_check_modules(GIO REQUIRED gio-2.0>=${GLIB_REQUIRED})
pkg_check_modules(GNUTLS REQUIRED gnutls>=${GNUTLS_REQUIRED})
pkg_check_modules(SOUP REQUIRED libsoup-2.4>=${SOUP_REQUIRED})
pkg_check_modules(NICE REQUIRED nice>=${NICE_REQUIRED})
pkg_check_modules(OPENCV REQUIRED opencv>=${OPENCV_REQUIRED})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

include(FindLibSctp)
include(FindLibVpx)

find_program(VALGRIND valgrind)

set (VERSION ${PROJECT_VERSION})
set (PACKAGE ${PROJECT_NAME})
set (GETTEXT_PACKAGE "gst-kurento-plugins")

include(GNUInstallDirs)

set (CMAKE_INSTALL_MODULESDIR ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
#TODO: Enable -Wall once code is fixec
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DHAVE_CONFIG_H -Werror -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DHAVE_CONFIG_H -Werror")

enable_testing()

add_subdirectory(kms-dtls-plugins)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)
# add_subdirectory(tests)

message(STATUS "Compiling : ${PROJECT_NAME}-${PROJECT_VERSION}")
