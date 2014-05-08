# - Try to find LibTesseract
# Once done this will define
#
#  TESSERACT_FOUND - system has LibTesseract
#  TESSERACT_INCLUDE_DIRS - the LibTesseract include directory
#  TESSERACT_LIBRARIES - the libraries needed to use LibTesseract

find_path(TESSERACT_INCLUDE_DIRS
    NAMES tesseract/capi.h
)

find_library(TESSERACT_LIBRARIES
    NAMES tesseract
)

if ((NOT ${TESSERACT_INCLUDE_DIRS} MATCHES ".*NOTFOUND") AND (NOT ${TESSERACT_LIBRARIES} MATCHES ".*NOTFOUND"))
  set (TESSERACT_FOUND "1")
else ()
  message (SEND_ERROR "tesseract not found")
endif ()
