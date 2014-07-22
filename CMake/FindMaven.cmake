# - Try to find Maven
# - Copied from one of cmake templates
# -------------------------
# Once done this will define
#
#  Maven_FOUND - system has Maven
#  Maven_EXECUTABLE - the Maven executable

#=============================================================================
# Copyright 2014 Kurento
#
#=============================================================================

FIND_PROGRAM(Maven_EXECUTABLE NAMES mvn
        HINTS ENV${Maven_ROOT}/mvn ${Maven_ROOT}/mvn)

# handle the QUIETLY and REQUIRED arguments and set Maven_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Maven DEFAULT_MSG Maven_EXECUTABLE)

MARK_AS_ADVANCED(Maven_EXECUTABLE)
