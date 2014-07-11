cmake_minimum_required(VERSION 2.8)

project ("kms-core")

message ("If KurentoHelpers is not found, then it is necessary to install
          kms-cmake-utils package from the kurento repository")

find_package(KurentoHelpers REQUIRED)

include (KurentoGitHelpers)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

get_git_version (PROJECT_VERSION 0.0.1-dev)
install_git_hook (pre-commit ${CMAKE_CURRENT_SOURCE_DIR}/hooks/pre-commit.hook)

string(REPLACE "." ";" VERSION_LIST ${PROJECT_VERSION})
list(GET VERSION_LIST 0 PROJECT_VERSION_MAJOR)
list (LENGTH VERSION_LIST _len)
if (${_len} GREATER 1)
  list(GET VERSION_LIST 1 PROJECT_VERSION_MINOR)
endif ()
if (${_len} GREATER 2)
  list(GET VERSION_LIST 2 PROJECT_VERSION_PATCH)
endif ()

find_package(PkgConfig)

set (GST_REQUIRED 1.3.3)
set (GLIB_REQUIRED 2.38)

set (JSONRPC_REQUIRED 0.0.6)
set (SIGCPP_REQUIRED 2.0.10)
set (GLIBMM_REQUIRED 2.37)

#gst-plugins dependencies
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-base-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_CHECK REQUIRED gstreamer-check-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_SDP REQUIRED gstreamer-sdp-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_PBUTILS REQUIRED gstreamer-pbutils-1.0>=${GST_REQUIRED})

#server dependencies
pkg_check_modules(JSONRPC REQUIRED libjsonrpc>=${JSONRPC_REQUIRED})
pkg_check_modules(SIGCPP REQUIRED sigc++-2.0>=${SIGCPP_REQUIRED})
pkg_check_modules(GLIBMM REQUIRED glibmm-2.4>=${GLIBMM_REQUIRED})

set (VERSION ${PROJECT_VERSION})
set (PACKAGE ${PROJECT_NAME})
set (GETTEXT_PACKAGE "kms-core")
set (MANUAL_CHECK OFF CACHE BOOL "Tests will generate files")
set (ENABLE_DEBUGGING_TESTS OFF CACHE BOOL "Enable test that are not yet stable")

include(GNUInstallDirs)

set (CMAKE_INSTALL_MODULESDIR ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DHAVE_CONFIG_H -Werror -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DHAVE_CONFIG_H -Wall -Werror -std=c++11")

enable_testing()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)
add_subdirectory(tests)

message(STATUS "Compiling : ${PROJECT_NAME}-${PROJECT_VERSION}")
