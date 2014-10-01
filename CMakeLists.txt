cmake_minimum_required(VERSION 2.8)

project ("kms-core")

message ("If KurentoHelpers is not found, then it is necessary to install
          kms-cmake-utils package from the kurento repository")

find_package(KurentoHelpers REQUIRED)

include (KurentoGitHelpers)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
include (CodeGenerator)

get_values_from_model(PREFIX VALUE MODELS ${CMAKE_SOURCE_DIR}/src/server/interface KEYS version)

get_git_version (PROJECT_VERSION ${VALUE_VERSION})
install_git_hook (pre-commit ${CMAKE_CURRENT_SOURCE_DIR}/hooks/pre-commit.hook)

find_package(PkgConfig)

set (GST_REQUIRED 1.4.0)
set (GLIB_REQUIRED 2.38)

find_package(Boost REQUIRED COMPONENTS filesystem system)

#gst-plugins dependencies
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-base-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_CHECK REQUIRED gstreamer-check-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_SDP REQUIRED gstreamer-sdp-1.0>=${GST_REQUIRED})
pkg_check_modules(GSTREAMER_PBUTILS REQUIRED gstreamer-pbutils-1.0>=${GST_REQUIRED})

find_package(KmsJsonRpc 0.1.0 REQUIRED)
pkg_check_modules(SIGCPP REQUIRED sigc++-2.0>=2.0.10)
pkg_check_modules(GLIBMM REQUIRED glibmm-2.4>=2.37)

find_package(LibVpx REQUIRED)

set (VERSION ${PROJECT_VERSION})
set (PACKAGE ${PROJECT_NAME})
set (GETTEXT_PACKAGE "kms-core")
set (MANUAL_CHECK OFF CACHE BOOL "Tests will generate files")
set (ENABLE_DEBUGGING_TESTS OFF CACHE BOOL "Enable test that are not yet stable")

include(GNUInstallDirs)

set (CMAKE_INSTALL_GST_PLUGINS_DIR ${CMAKE_INSTALL_LIBDIR}/gstreamer-1.0)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DHAVE_CONFIG_H -Werror -Wall -Werror=declaration-after-statement")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DHAVE_CONFIG_H -Wall -Werror -std=c++11")

enable_testing()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)
add_subdirectory(tests)

message(STATUS "Compiling : ${PROJECT_NAME}-${PROJECT_VERSION}")

install(
  FILES "${CMAKE_SOURCE_DIR}/CMake/CodeGenerator.cmake"
  DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)

install(
  FILES "${CMAKE_SOURCE_DIR}/CMake/FindMaven.cmake"
  DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)

install(
  FILES "${CMAKE_SOURCE_DIR}/CMake/FindNpm.cmake"
  DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)
