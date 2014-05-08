# - Try to find LibVpx
# Once done this will define
#
#  VPX_FOUND - system has LibVpx
#  VPX_INCLUDE_DIRS - the LibVpx include directory
#  VPX_LIBRARIES - the libraries needed to use LibVpx

find_path(VPX_INCLUDE_DIRS
    NAMES vpx/vp8.h
)

find_library(VPX_LIBRARIES
    NAMES vpx
)

if ((NOT ${VPX_INCLUDE_DIRS} MATCHES ".*NOTFOUND") AND (NOT ${VPX_LIBRARIES} MATCHES ".*NOTFOUND"))
  set (VPX_FOUND "1")
else ()
  message (SEND_ERROR "libvpx not found")
endif ()
