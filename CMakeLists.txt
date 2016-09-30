cmake_minimum_required(VERSION 2.8)

project ("kms-core")

message ("If KurentoHelpers is not found, then it is necessary to install
          kms-cmake-utils package from the kurento repository")

find_package(KurentoHelpers REQUIRED)

include (KurentoGitHelpers)
if (DEFINED CMAKE_MODULE_PATH)
  list(REVERSE CMAKE_MODULE_PATH)
endif ()
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
include (CodeGenerator)

get_values_from_model(PREFIX VALUE MODELS ${CMAKE_CURRENT_SOURCE_DIR}/src/server/interface KEYS version)

get_git_version (PROJECT_VERSION ${VALUE_VERSION})
install_git_hook (pre-commit ${CMAKE_CURRENT_SOURCE_DIR}/hooks/pre-commit.hook)

set (GST_REQUIRED ^1.7.1)
set (JSON_RPC_REQUIRED ^1.1.0)
set (SIGCPP_REQUIRED ^2.0.10)
set (GLIBMM_REQUIRED ^2.37)

include (GenericFind)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DHAVE_CONFIG_H -Werror -Wall -Werror=declaration-after-statement -Wno-deprecated-declarations -O2 -std=c11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DHAVE_CONFIG_H -Wall -Werror -std=c++11 -O2")

include (CheckCXXSourceCompiles)

set (CMAKE_REQUIRED_FLAGS ${CMAKE_CXX_FLAGS})

check_cxx_source_compiles (
  "#include <regex>
  #include <iostream>
  int main () {
    std::regex re (\"//*\");
    std::string orig = \"//\";

    std::cout << std::regex_replace (orig, re, \"/\") << std::endl;

    return 0;
  }
"
  REGEX_REPLACE_EXISTS
)

if (${REGEX_REPLACE_EXISTS})
  set (BOOST_REQUIRED_COMPONENTS "filesystem system")
  generic_find (LIBNAME Boost REQUIRED COMPONENTS filesystem system unit_test_framework)
else()
  set (BOOST_REQUIRED_COMPONENTS "filesystem system regex")
  generic_find (LIBNAME Boost REQUIRED COMPONENTS filesystem system regex unit_test_framework)
endif()

#gst-plugins dependencies
generic_find (LIBNAME gstreamer-1.5 VERSION ${GST_REQUIRED} REQUIRED)
generic_find (LIBNAME gstreamer-base-1.5 VERSION ${GST_REQUIRED} REQUIRED)
generic_find (LIBNAME gstreamer-video-1.5 VERSION ${GST_REQUIRED} REQUIRED)
generic_find (LIBNAME gstreamer-check-1.5 VERSION ${GST_REQUIRED})
generic_find (LIBNAME gstreamer-sdp-1.5 VERSION ${GST_REQUIRED} REQUIRED)
generic_find (LIBNAME gstreamer-pbutils-1.5 VERSION ${GST_REQUIRED} REQUIRED)
generic_find (LIBNAME gstreamer-rtp-1.5 VERSION ${GST_REQUIRED} REQUIRED)

generic_find (LIBNAME KmsJsonRpc VERSION ${JSON_RPC_REQUIRED} REQUIRED)
generic_find (LIBNAME sigc++-2.0 VERSION ${SIGCPP_REQUIRED} REQUIRED)
generic_find (LIBNAME glibmm-2.4 VERSION ${GLIBMM_REQUIRED} REQUIRED)
generic_find (LIBNAME uuid REQUIRED)

find_package(LibVpx REQUIRED)

set (VERSION ${PROJECT_VERSION})
set (PACKAGE ${PROJECT_NAME})
set (GETTEXT_PACKAGE "kms-core")
set (MANUAL_CHECK OFF CACHE BOOL "Tests will generate files")
set (ENABLE_DEBUGGING_TESTS OFF CACHE BOOL "Enable test that are not yet stable")

include(GNUInstallDirs)

set (CMAKE_INSTALL_GST_PLUGINS_DIR ${CMAKE_INSTALL_LIBDIR}/gstreamer-1.5)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG("-Wno-error=date-time" COMPILER_SUPPORTS_DATETIME_ERROR)
if (COMPILER_SUPPORTS_DATETIME_ERROR)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=date-time")
endif ()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

message(STATUS "Compiling : ${PROJECT_NAME}-${PROJECT_VERSION}")

install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/CMake/CodeGenerator.cmake"
  DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)

install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/CMake/FindMaven.cmake"
  DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)

install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/CMake/FindNpm.cmake"
  DESTINATION ${CMAKE_MODULES_INSTALL_DIR}
)
