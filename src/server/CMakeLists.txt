cmake_minimum_required (VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

include (CodeGenerator)

set (KMS_CORE_INTERFACE_SOURCES
)
set (KMS_CORE_INTERFACE_HEADERS
  interface/KurentoException.hpp
)

set (KMS_CORE_IMPL_SOURCES
  implementation/EventHandler.cpp
  implementation/Factory.cpp
  implementation/MediaSet.cpp
  implementation/ModuleManager.cpp
)
set (KMS_CORE_IMPL_HEADERS
  implementation/EventHandler.hpp
  implementation/Factory.hpp
  implementation/MediaSet.hpp
  implementation/FactoryRegistrar.hpp
  implementation/ModuleManager.hpp
)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

generate_code (
  MODELS ${CMAKE_CURRENT_SOURCE_DIR}/interface
  INTERFACE_LIB_EXTRA_SOURCES ${KMS_CORE_INTERFACE_SOURCES}
  INTERFACE_LIB_EXTRA_HEADERS ${KMS_CORE_INTERFACE_HEADERS}
  INTERFACE_LIB_EXTRA_INCLUDE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}/interface
      ${KmsJsonRpc_INCLUDE_DIRS}
      ${SIGCPP_INCLUDE_DIRS}
      ${GLIBMM_INCLUDE_DIRS}
  INTERFACE_LIB_EXTRA_LIBRARIES
      ${KmsJsonRpc_LIBRARIES}
      ${SIGCPP_LIBRARIES}
      ${GLIBMM_LIBRARIES}
  SERVER_IMPL_LIB_EXTRA_SOURCES
      ${KMS_CORE_IMPL_SOURCES}
  SERVER_IMPL_LIB_EXTRA_HEADERS
      ${KMS_CORE_IMPL_HEADERS}
  SERVER_IMPL_LIB_EXTRA_INCLUDE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}/implementation
      ${Boost_INCLUDE_DIRS}
      ${Boost_FILESYSTEM_INCLUDE_DIRS}
      ${Boost_SYSTEM_INCLUDE_DIRS}
      ${GSTREAMER_INCLUDE_DIRS}
      ${KmsJsonRpc_INCLUDE_DIRS}
  SERVER_IMPL_LIB_EXTRA_LIBRARIES
      ${GSTREAMER_SDP_LIBRARIES}
      ${Boost_LIBRARIES}
      ${Boost_FILESYSTEM_LIBRARIES}
      ${Boost_SYSTEM_LIBRARIES}
      ${GSTREAMER_LIBRARIES}
      ${KmsJsonRpc_LIBRARIES}
  MODULE_EXTRA_INCLUDE_DIRS
      ${GSTREAMER_INCLUDE_DIRS}
      ${KmsJsonRpc_INCLUDE_DIRS}
      ${SIGCPP_INCLUDE_DIRS}
  MODULE_EXTRA_LIBRARIES
      ${GSTREAMER_LIBRARIES}
      ${KmsJsonRpc_LIBRARIES}
      ${SIGCPP_LIBRARIES}
  SERVER_STUB_DESTINATION
      ${CMAKE_CURRENT_SOURCE_DIR}/implementation/objects
  SERVER_IMPL_LIB_PKGCONFIG_EXTRA_REQUIRES
      "gstreamer-1.0 >= ${GST_REQUIRED} gstreamer-sdp-1.0 >= ${GST_REQUIRED} libjsonrpc >= ${JSON_RPC_REQUIRED} sigc++-2.0 >= ${SIGCPP_REQUIRED} glibmm-2.4 >= ${GLIBMM_REQUIRED}"
)
