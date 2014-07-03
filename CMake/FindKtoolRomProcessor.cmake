# - Try to find KTOOL_ROM_PROCESSOR
# - Copied from one of cmake templates
# -------------------------
# Once done this will define
#
#  KTOOL_ROM_PROCESSOR_FOUND - system has KTOOL_ROM_PROCESSOR
#  KTOOL_ROM_PROCESSOR_EXECUTABLE - the KTOOL_ROM_PROCESSOR executable

#=============================================================================
# Copyright 2006-2009 Kitware, Inc.
# Copyright 2006 Alexander Neundorf <neundorf@kde.org>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

set (KTOOL_ROM_PROCESSOR_ROOT /usr/bin CACHE STRING "ktool-rom-processor directory")

FIND_PROGRAM(KTOOL_ROM_PROCESSOR_EXECUTABLE NAMES ktool-rom-processor
        HINTS ENV${KTOOL_ROM_PROCESSOR_ROOT}/ktool-rom-processor ${KTOOL_ROM_PROCESSOR_ROOT}/ktool-rom-processor)

# handle the QUIETLY and REQUIRED arguments and set KTOOL_ROM_PROCESSOR_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(KTOOL_ROM_PROCESSOR DEFAULT_MSG KTOOL_ROM_PROCESSOR_EXECUTABLE)

MARK_AS_ADVANCED(KTOOL_ROM_PROCESSOR_EXECUTABLE)
