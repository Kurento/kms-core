dnl required version of autoconf
AC_PREREQ([2.53])

dnl TODO: fill in your package name and package version here
AC_INIT([gst-kurento-plugins],[1.0.5])

dnl required versions of gstreamer and plugins-base
GST_REQUIRED=1.0.5
GSTPB_REQUIRED=1.0.5
GLIB_REQ=2.34
NICE_REQUIRED=0.1.2

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

dnl required version of automake
AM_INIT_AUTOMAKE([1.10])

dnl enable mainainer mode by default
AM_MAINTAINER_MODE([enable])

dnl check for tools (compiler etc.)
AC_PROG_CC
AC_PROG_CC_STDC

dnl check if the compiler supports '-c' and '-o' options
AM_PROG_CC_C_O

dnl required version of libtool
LT_PREREQ([2.2.6])
LT_INIT

AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

AG_GST_GLIB_CHECK([$GLIB_REQ])

dnl give error and exit if we don't have pkgconfig
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([You need to have pkg-config installed!])
])

dnl Check for the required version of GStreamer core (and gst-plugins-base)
dnl This will export GST_CFLAGS and GST_LIBS variables for use in Makefile.am
dnl
dnl If you need libraries from gst-plugins-base here, also add:
dnl for libgstaudio-1.0: gstreamer-audio-1.0 >= $GST_REQUIRED
dnl for libgstvideo-1.0: gstreamer-video-1.0 >= $GST_REQUIRED
dnl for libgsttag-1.0: gstreamer-tag-1.0 >= $GST_REQUIRED
dnl for libgstpbutils-1.0: gstreamer-pbutils-1.0 >= $GST_REQUIRED
dnl for libgstfft-1.0: gstreamer-fft-1.0 >= $GST_REQUIRED
dnl for libgstinterfaces-1.0: gstreamer-interfaces-1.0 >= $GST_REQUIRED
dnl for libgstrtp-1.0: gstreamer-rtp-1.0 >= $GST_REQUIRED
dnl for libgstrtsp-1.0: gstreamer-rtsp-1.0 >= $GST_REQUIRED
dnl etc.

PKG_CHECK_MODULES(GST, [
  gstreamer-1.0 >= $GST_REQUIRED
  gstreamer-base-1.0 >= $GST_REQUIRED
  gstreamer-controller-1.0 >= $GST_REQUIRED
  gstreamer-sdp-1.0 >= $GST_REQUIRED
  gstreamer-video-1.0 >= $GST_REQUIRED
  gstreamer-pbutils-1.0 >= $GST_REQUIRED
], [
], [
  AC_MSG_ERROR([
      You need to install or upgrade the GStreamer development
      packages on your system. On debian-based systems these are
      libgstreamer1.0-dev and libgstreamer-plugins-base1.0-dev.
      on RPM-based systems gstreamer1.0-devel, libgstreamer1.0-devel
      or similar. The minimum version required is $GST_REQUIRED.
  ])
])

PKG_CHECK_MODULES(GIO, [
  gio-2.0 >= $GLIB_REQUIRED
], [
], [
  AC_MSG_ERROR([
      You need to install or upgrade the Gio development
      packages on your system to version $GIO_REQUIRED.
  ])
])

PKG_CHECK_MODULES(NICE, [
  nice >= $NICE_REQUIRED
], [
], [
  AC_MSG_ERROR([
      You need to install or upgrade the nice development
      packages on your system to version $NICE_REQUIRED.
  ])
])

dnl *** opencv ***
PKG_CHECK_MODULES(OPENCV, opencv >= 2.0.0 opencv <= 2.4.3 , [
  AC_CHECK_HEADERS([highgui.h opencv2/highgui/highgui_c.h])
  AC_LANG([C])

  if test $ac_cv_header_highgui_h = "yes" -o $ac_cv_header_opencv2_highgui_highgui_c_h = "yes" ; then
    HAVE_OPENCV="yes"
  else
    AC_MSG_RESULT([neither highgui.h nor opencv2/highgui/highgui_c.h could not be found])
    HAVE_OPENCV="no"
  fi
], [
  HAVE_OPENCV="no"
])
AM_CONDITIONAL(HAVE_OPENCV, test "x$HAVE_OPENCV" = "xyes")

dnl *** tesseract ***
AC_CHECK_LIB([tesseract], [TessBaseAPICreate],
  [
    AC_CHECK_HEADER([tesseract/capi.h],
      [
        HAVE_TESSERACT="yes"
      ]
    )
  ], [
    AC_MSG_WARN([Could not find tesseract library, plate detector will not be compiled])
  ]
)
AM_CONDITIONAL(HAVE_TESSERACT, test "x$HAVE_TESSERACT" = "xyes")

PKG_CHECK_MODULES(GST_CHECK, [
  gstreamer-check-1.0 >= $GST_REQUIRED
], [
  HAVE_GST_CHECK="yes"
], [
  HAVE_GST_CHECK="no"
  AC_MSG_NOTICE([no gstreamer-check-1.0 >= $GST_REQUIRED (GStreamer Check unittest Library) found])
])
AM_CONDITIONAL(HAVE_GST_CHECK, test "x$HAVE_GST_CHECK" = "xyes")

dnl check if compiler understands -Wall (if yes, add -Wall to GST_CFLAGS)
AC_MSG_CHECKING([to see if compiler understands -Wall])
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wall"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([ ], [ ])], [
  GST_CFLAGS="$GST_CFLAGS -Wall"
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
])

dnl set the plugindir where plugins should be installed (for src/Makefile.am)
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-1.0/plugins"
else
  plugindir="\$(libdir)/gstreamer-1.0"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

dnl FAILED TESTS

dnl enable/disable automuxer test
AC_ARG_ENABLE([automuxertest],
[  --enable-automuxertest    Turn on testing automuxer],
[case "${enableval}" in
  yes) automuxertest=true ;;
  no)  automuxertest=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-automuxertest]) ;;
esac],[automuxertest=false])
AM_CONDITIONAL([AUTOMUXERTEST], [test x$automuxertest = xtrue])

dnl enable/disable rtpendpoint_test_vp8_sendonly_play_after_negotiation test
AC_ARG_ENABLE([rtpendpoint_test_vp8_sendonly_play_after_negotiation],
[  --enable-rtpendpoint-test-vp8-sendonly-play-after-negotiation]    Turn on testing rtpendpoint vp8 sendonly play after negotiation],
[case "${enableval}" in
  yes) rtpendpoint_test_vp8_sendonly_play_after_negotiation=true ;;
  no)  rtpendpoint_test_vp8_sendonly_play_after_negotiation=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-rtpendpoint-test-vp8-sendonly-play-after-negotiation]) ;;
esac],[rtpendpoint_test_vp8_sendonly_play_after_negotiation=false])
AM_CONDITIONAL([RTPENDPOINT_TEST_VP8_SENDONLY_PLAY_AFTER_NEGOTIATION], [test x$rtpendpoint_test_vp8_sendonly_play_after_negotiation = xtrue])

if test "x${FINAL_INSTALL_DIR}" = "x"; then
  AC_DEFINE([FINAL_INSTALL_DIR], [""],"Final installation Path")
else
  AC_DEFINE_UNQUOTED([FINAL_INSTALL_DIR], ["$FINAL_INSTALL_DIR"],"Final installation Path")
fi

AC_CONFIG_FILES([Makefile src/Makefile src/filters/Makefile tests/Makefile tests/check/Makefile src/filters/platedetector/Makefile src/filters/jackvader/Makefile src/filters/h264filter/Makefile])
AC_OUTPUT
