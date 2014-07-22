# - Try to find Npm
# - Copied from one of cmake templates
# -------------------------
# Once done this will define
#
#  Npm_FOUND - system has Npm
#  Npm_EXECUTABLE - the Npm executable

#=============================================================================
# Copyright 2014 Kurento
#
#=============================================================================

FIND_PROGRAM(Npm_EXECUTABLE NAMES npm
        HINTS ENV${Npm_ROOT}/npm ${Npm_ROOT}/npm)

# handle the QUIETLY and REQUIRED arguments and set Npm_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Npm DEFAULT_MSG Npm_EXECUTABLE)

MARK_AS_ADVANCED(Npm_EXECUTABLE)
