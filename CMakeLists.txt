cmake_minimum_required(VERSION 2.8)

project ("gst-kurento-plugins")

message ("If KurentoHelpers is not found, then it is necessary to install
          kms-cmake-utils package from the kurento repository")

find_package(KurentoHelpers REQUIRED)

include (KurentoGitHelpers)

get_git_version (PROJECT_VERSION 1.2.14-dev)
install_git_hook (pre-commit ${CMAKE_CURRENT_SOURCE_DIR}/hooks/pre-commit.hook)

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

find_package(LibSctp REQUIRED)
find_package(LibVpx REQUIRED)
find_package(LibTesseract REQUIRED)

find_program(VALGRIND valgrind)

set (VERSION ${PROJECT_VERSION})
set (PACKAGE ${PROJECT_NAME})
set (GETTEXT_PACKAGE "gst-kurento-plugins")
set (DATAROOTDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR})
set (MANUAL_CHECK OFF CACHE BOOL "Tests will generate files")
set (ENABLE_DEBUGGING_TESTS OFF CACHE BOOL "Enable test that are not yet stable")
set (BINARY_LOCATION "http://files.kurento.org/" CACHE STRING "Local binary directory expressed as an URI ( http:// or file:/// )")
message ("Binary location: " ${BINARY_LOCATION})

include(GNUInstallDirs)

set (CMAKE_INSTALL_MODULESDIR ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DHAVE_CONFIG_H -Werror -Wall")

set (ENABLE_AUTOMUXER_TESTS FALSE CACHE BOOL "Enable automuxer tests")
set (ENABLE_DEBUGGING_TESTS FALSE CACHE BOOL "Enable debugging tests")
set (ENABLE_INTEGRATION_TESTS FALSE CACHE BOOL "Enable integration tests")
set (ENABLE_MEMORY_LEAKS_TESTS FALSE CACHE BOOL "Enable memory leaks tests")
set (DISABLE_TESTS FALSE CACHE BOOL "Disable make check")

if (${ENABLE_DEBUGGING_TESTS})
  message (STATUS "Enabling debugging tests")
  add_definitions(-DDEBUGGING_TESTS -DENABLE_DEBUGGING_TESTS)
endif()

enable_testing()

add_subdirectory(kms-dtls-plugins)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)
add_subdirectory(tests)

message(STATUS "Compiling : ${PROJECT_NAME}-${PROJECT_VERSION}")
