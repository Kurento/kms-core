# - Try to find LibSctp
# Once done this will define
#
#  SCTP_FOUND - system has LibSctp
#  SCTP_INCLUDE_DIRS - the LibSctp include directory
#  SCTP_LIBRARIES - the libraries needed to use LibSctp

find_path(SCTP_INCLUDE_DIRS
    NAMES netinet/sctp.h
)

find_library(SCTP_LIBRARIES
    NAMES sctp
)

if ((NOT ${SCTP_INCLUDE_DIRS} MATCHES ".*NOTFOUND") AND (NOT ${SCTP_LIBRARIES} MATCHES ".*NOTFOUND"))
  set (SCTP_FOUND "1")
else ()
  message (SEND_ERROR "libsctp not found")
endif ()
