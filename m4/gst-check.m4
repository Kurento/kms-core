dnl pkg-config-based checks for GStreamer modules and dependency modules

dnl generic:
dnl AG_GST_PKG_CHECK_MODULES([PREFIX], [WHICH], [REQUIRED])
dnl sets HAVE_[$PREFIX], [$PREFIX]_*
dnl AG_GST_CHECK_MODULES([PREFIX], [MODULE], [MINVER], [NAME], [REQUIRED])
dnl sets HAVE_[$PREFIX], [$PREFIX]_*

dnl specific:
dnl AG_GST_CHECK_GST([MAJMIN], [MINVER], [REQUIRED])
dnl   also sets/ACSUBSTs GST_TOOLS_DIR and GST_PLUGINS_DIR
dnl AG_GST_CHECK_GST_BASE([MAJMIN], [MINVER], [REQUIRED])
dnl AG_GST_CHECK_GST_CONTROLLER([MAJMIN], [MINVER], [REQUIRED])
dnl AG_GST_CHECK_GST_NET([MAJMIN], [MINVER], [REQUIRED])
dnl AG_GST_CHECK_GST_CHECK([MAJMIN], [MINVER], [REQUIRED])
dnl AG_GST_CHECK_GST_PLUGINS_BASE([MAJMIN], [MINVER], [REQUIRED])
dnl   also sets/ACSUBSTs GSTPB_PLUGINS_DIR

AC_DEFUN([AG_GST_PKG_CHECK_MODULES],
[
  which="[$2]"
  dnl not required by default, since we use this mostly for plugin deps
  required=ifelse([$3], , "no", [$3])

  PKG_CHECK_MODULES([$1], $which,
    [
      HAVE_[$1]="yes"
    ],
    [
      HAVE_[$1]="no"
      if test "x$required" = "xyes"; then
        AC_MSG_ERROR($[$1]_PKG_ERRORS)
      else
        AC_MSG_NOTICE($[$1]_PKG_ERRORS)
      fi
    ])

  dnl AC_SUBST of CFLAGS and LIBS was not done before automake 1.7
  dnl It gets done automatically in automake >= 1.7, which we now require
]))

AC_DEFUN([AG_GST_CHECK_MODULES],
[
  module=[$2]
  minver=[$3]
  name="[$4]"
  required=ifelse([$5], , "yes", [$5]) dnl required by default

  PKG_CHECK_MODULES([$1], $module >= $minver,
    [
      HAVE_[$1]="yes"
    ],
    [
      HAVE_[$1]="no"
      AC_MSG_NOTICE($[$1]_PKG_ERRORS)
      if test "x$required" = "xyes"; then
        AC_MSG_ERROR([no $module >= $minver ($name) found])
      else
        AC_MSG_NOTICE([no $module >= $minver ($name) found])
      fi
    ])

  dnl AC_SUBST of CFLAGS and LIBS was not done before automake 1.7
  dnl It gets done automatically in automake >= 1.7, which we now require
]))

AC_DEFUN([AG_GST_CHECK_GST],
[
  AG_GST_CHECK_MODULES(GST, gstreamer-[$1], [$2], [GStreamer], [$3])
  dnl allow setting before calling this macro to override
  if test -z $GST_TOOLS_DIR; then
    GST_TOOLS_DIR=`$PKG_CONFIG --variable=toolsdir gstreamer-[$1]`
    if test -z $GST_TOOLS_DIR; then
      AC_MSG_ERROR(
        [no tools dir set in GStreamer pkg-config file, core upgrade needed.])
    fi
  fi
  AC_MSG_NOTICE([using GStreamer tools in $GST_TOOLS_DIR])
  AC_SUBST(GST_TOOLS_DIR)

  dnl check for where core plug-ins got installed
  dnl this is used for unit tests
  dnl allow setting before calling this macro to override
  if test -z $GST_PLUGINS_DIR; then
    GST_PLUGINS_DIR=`$PKG_CONFIG --variable=pluginsdir gstreamer-[$1]`
    if test -z $GST_PLUGINS_DIR; then
      AC_MSG_ERROR(
        [no pluginsdir set in GStreamer pkg-config file, core upgrade needed.])
    fi
  fi
  AC_MSG_NOTICE([using GStreamer plug-ins in $GST_PLUGINS_DIR])
  AC_SUBST(GST_PLUGINS_DIR)
])

AC_DEFUN([AG_GST_CHECK_GST_BASE],
[
  AG_GST_CHECK_MODULES(GST_BASE, gstreamer-base-[$1], [$2],
    [GStreamer Base Libraries], [$3])
])

AC_DEFUN([AG_GST_CHECK_GST_CONTROLLER],
[
  AG_GST_CHECK_MODULES(GST_CONTROLLER, gstreamer-controller-[$1], [$2],
    [GStreamer Controller Library], [$3])
])

AC_DEFUN([AG_GST_CHECK_GST_NET],
[
  AG_GST_CHECK_MODULES(GST_NET, gstreamer-net-[$1], [$2],
    [GStreamer Network Library], [$3])
])

AC_DEFUN([AG_GST_CHECK_GST_CHECK],
[
  AG_GST_CHECK_MODULES(GST_CHECK, gstreamer-check-[$1], [$2],
    [GStreamer Check unittest Library], [$3])
])

dnl ===========================================================================
dnl AG_GST_CHECK_UNINSTALLED_SETUP([ACTION-IF-UNINSTALLED], [ACTION-IF-NOT])
dnl
dnl ACTION-IF-UNINSTALLED  (optional) extra actions to perform if the setup
dnl                        is an uninstalled setup
dnl ACTION-IF-NOT          (optional) extra actions to perform if the setup
dnl                        is not an uninstalled setup
dnl ===========================================================================
AC_DEFUN([AG_GST_CHECK_UNINSTALLED_SETUP],
[
  AC_MSG_CHECKING([whether this is an uninstalled GStreamer setup])
  AC_CACHE_VAL(gst_cv_is_uninstalled_setup,[
    gst_cv_is_uninstalled_setup=no
    if (set -u; : $GST_PLUGIN_SYSTEM_PATH) 2>/dev/null ; then
      if test -z "$GST_PLUGIN_SYSTEM_PATH" \
           -a -n "$GST_PLUGIN_SCANNER"     \
           -a -n "$GST_PLUGIN_PATH"        \
           -a -n "$GST_REGISTRY"           \
           -a -n "$DYLD_LIBRARY_PATH"      \
           -a -n "$LD_LIBRARY_PATH"; then
        gst_cv_is_uninstalled_setup=yes;
      fi
    fi
  ])
  AC_MSG_RESULT($gst_cv_is_uninstalled_setup)
  if test "x$gst_cv_is_uninstalled_setup" = "xyes"; then
    ifelse([$1], , :, [$1])
  else
    ifelse([$2], , :, [$2])
  fi
])

dnl ===========================================================================
dnl AG_GST_CHECK_GST_PLUGINS_BASE([GST-API_VERSION], [MIN-VERSION], [REQUIRED])
dnl
dnl Sets GST_PLUGINS_BASE_CFLAGS and GST_PLUGINS_BASE_LIBS.
dnl
dnl Also sets GSTPB_PLUGINS_DIR (and for consistency also GST_PLUGINS_BASE_DIR)
dnl for use in Makefile.am. This is only really needed/useful in uninstalled
dnl setups, since in an installed setup all plugins will be found in
dnl GST_PLUGINS_DIR anyway.
dnl ===========================================================================
AC_DEFUN([AG_GST_CHECK_GST_PLUGINS_BASE],
[
  AG_GST_CHECK_MODULES(GST_PLUGINS_BASE, gstreamer-plugins-base-[$1], [$2],
    [GStreamer Base Plugins], [$3])

  if test "x$HAVE_GST_PLUGINS_BASE" = "xyes"; then
    dnl check for where base plugins got installed
    dnl this is used for unit tests
    dnl allow setting before calling this macro to override
    if test -z $GSTPB_PLUGINS_DIR; then
      GSTPB_PLUGINS_DIR=`$PKG_CONFIG --variable=pluginsdir gstreamer-plugins-base-[$1]`
      if test -z $GSTPB_PLUGINS_DIR; then
        AC_MSG_ERROR(
          [no pluginsdir set in GStreamer Base Plugins pkg-config file])
      fi
    fi
    AC_MSG_NOTICE([using GStreamer Base Plugins in $GSTPB_PLUGINS_DIR])
    GST_PLUGINS_BASE_DIR="$GSTPB_PLUGINS_DIR/gst:$GSTPB_PLUGINS_DIR/sys:$GSTPB_PLUGINS_DIR/ext"
    AC_SUBST(GST_PLUGINS_BASE_DIR)
    AC_SUBST(GSTPB_PLUGINS_DIR)
  fi
])

dnl ===========================================================================
dnl AG_GST_CHECK_GST_PLUGINS_GOOD([GST-API_VERSION], [MIN-VERSION])
dnl
dnl Will set GST_PLUGINS_GOOD_DIR for use in Makefile.am. Note that this will
dnl only be set in an uninstalled setup, since -good ships no .pc file and in
dnl an installed setup all plugins will be found in GST_PLUGINS_DIR anyway.
dnl ===========================================================================
AC_DEFUN([AG_GST_CHECK_GST_PLUGINS_GOOD],
[
  AG_GST_CHECK_MODULES(GST_PLUGINS_GOOD, gstreamer-plugins-good-[$1], [$2],
    [GStreamer Good Plugins], [no])

  if test "x$HAVE_GST_PLUGINS_GOOD" = "xyes"; then
    dnl check for where good plugins got installed
    dnl this is used for unit tests
    dnl allow setting before calling this macro to override
    if test -z $GST_PLUGINS_GOOD_DIR; then
      GST_PLUGINS_GOOD_DIR=`$PKG_CONFIG --variable=pluginsdir gstreamer-plugins-good-[$1]`
      if test -z $GST_PLUGINS_GOOD_DIR; then
        AC_MSG_ERROR([no pluginsdir set in GStreamer Good Plugins pkg-config file])
      fi
    fi
    AC_MSG_NOTICE([using GStreamer Good Plugins in $GST_PLUGINS_GOOD_DIR])
    GST_PLUGINS_GOOD_DIR="$GST_PLUGINS_GOOD_DIR/gst:$GST_PLUGINS_GOOD_DIR/sys:$GST_PLUGINS_GOOD_DIR/ext"
    AC_SUBST(GST_PLUGINS_GOOD_DIR)
  fi
])

dnl ===========================================================================
dnl AG_GST_CHECK_GST_PLUGINS_UGLY([GST-API_VERSION], [MIN-VERSION])
dnl
dnl Will set GST_PLUGINS_UGLY_DIR for use in Makefile.am. Note that this will
dnl only be set in an uninstalled setup, since -bad ships no .pc file and in
dnl an installed setup all plugins will be found in GST_PLUGINS_DIR anyway.
dnl ===========================================================================
AC_DEFUN([AG_GST_CHECK_GST_PLUGINS_UGLY],
[
  AG_GST_CHECK_MODULES(GST_PLUGINS_UGLY, gstreamer-plugins-ugly-[$1], [$2],
    [GStreamer Ugly Plugins], [no])

  if test "x$HAVE_GST_PLUGINS_UGLY" = "xyes"; then
    dnl check for where ugly plugins got installed
    dnl this is used for unit tests
    dnl allow setting before calling this macro to override
    if test -z $GST_PLUGINS_UGLY_DIR; then
      GST_PLUGINS_UGLY_DIR=`$PKG_CONFIG --variable=pluginsdir gstreamer-plugins-ugly-[$1]`
      if test -z $GST_PLUGINS_UGLY_DIR; then
        AC_MSG_ERROR([no pluginsdir set in GStreamer Ugly Plugins pkg-config file])
      fi
    fi
    AC_MSG_NOTICE([using GStreamer Ugly Plugins in $GST_PLUGINS_UGLY_DIR])
    GST_PLUGINS_UGLY_DIR="$GST_PLUGINS_UGLY_DIR/gst:$GST_PLUGINS_UGLY_DIR/sys:$GST_PLUGINS_UGLY_DIR/ext"
    AC_SUBST(GST_PLUGINS_UGLY_DIR)
  fi
])

dnl ===========================================================================
dnl AG_GST_CHECK_GST_PLUGINS_BAD([GST-API_VERSION], [MIN-VERSION])
dnl
dnl Will set GST_PLUGINS_BAD_DIR for use in Makefile.am. Note that this will
dnl only be set in an uninstalled setup, since -ugly ships no .pc file and in
dnl an installed setup all plugins will be found in GST_PLUGINS_DIR anyway.
dnl ===========================================================================
AC_DEFUN([AG_GST_CHECK_GST_PLUGINS_BAD],
[
  AG_GST_CHECK_MODULES(GST_PLUGINS_BAD, gstreamer-plugins-bad-[$1], [$2],
    [GStreamer Bad Plugins], [no])

  if test "x$HAVE_GST_PLUGINS_BAD" = "xyes"; then
    dnl check for where bad plugins got installed
    dnl this is used for unit tests
    dnl allow setting before calling this macro to override
    if test -z $GST_PLUGINS_BAD_DIR; then
      GST_PLUGINS_BAD_DIR=`$PKG_CONFIG --variable=pluginsdir gstreamer-plugins-bad-[$1]`
      if test -z $GST_PLUGINS_BAD_DIR; then
        AC_MSG_ERROR([no pluginsdir set in GStreamer Bad Plugins pkg-config file])
      fi
    fi
    AC_MSG_NOTICE([using GStreamer Bad Plugins in $GST_PLUGINS_BAD_DIR])
    GST_PLUGINS_BAD_DIR="$GST_PLUGINS_BAD_DIR/gst:$GST_PLUGINS_BAD_DIR/sys:$GST_PLUGINS_BAD_DIR/ext"
    AC_SUBST(GST_PLUGINS_BAD_DIR)
  fi
])

dnl ===========================================================================
dnl AG_GST_CHECK_GST_PLUGINS_FFMPEG([GST-API_VERSION], [MIN-VERSION])
dnl
dnl Will set GST_PLUGINS_FFMPEG_DIR for use in Makefile.am. Note that this will
dnl only be set in an uninstalled setup, since -ffmpeg ships no .pc file and in
dnl an installed setup all plugins will be found in GST_PLUGINS_DIR anyway.
dnl ===========================================================================
AC_DEFUN([AG_GST_CHECK_GST_PLUGINS_FFMPEG],
[
  AG_GST_CHECK_MODULES(GST_PLUGINS_FFMPEG, gstreamer-plugins-ffmpeg-[$1], [$2],
    [GStreamer FFmpeg Plugins], [no])

  if test "x$HAVE_GST_PLUGINS_FFMPEG" = "xyes"; then
    dnl check for where ffmpeg plugins got installed
    dnl this is used for unit tests
    dnl allow setting before calling this macro to override
    if test -z $GST_PLUGINS_FFMPEG_DIR; then
      GST_PLUGINS_FFMPEG_DIR=`$PKG_CONFIG --variable=pluginsdir gstreamer-plugins-ffmpeg-[$1]`
      if test -z $GST_PLUGINS_FFMPEG_DIR; then
        AC_MSG_ERROR([no pluginsdir set in GStreamer FFmpeg Plugins pkg-config file])
      fi
    fi
    GST_PLUGINS_FFMPEG_DIR="$GST_PLUGINS_FFMPEG_DIR/ext/ffmpeg"
    AC_MSG_NOTICE([using GStreamer FFmpeg Plugins in $GST_PLUGINS_FFMPEG_DIR])
    AC_SUBST(GST_PLUGINS_FFMPEG_DIR)
  fi
])
