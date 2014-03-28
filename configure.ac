dnl required version of autoconf
AC_PREREQ([2.53])

dnl TODO: fill in your package name and package version here
AC_INIT([gst-kurento-plugins],[1.2.8-dev])

dnl required versions of gstreamer and plugins-base
GST_REQUIRED=1.2.0
GSTPB_REQUIRED=1.2.0
GLIB_REQ=2.38
NICE_REQUIRED=0.1.4
GNUTLS_MIN_REQUIRED=3.2.0
SOUP_REQUIRED=2.40

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

dnl required version of automake
AM_INIT_AUTOMAKE([1.10])

dnl enable mainainer mode by default
AM_MAINTAINER_MODE([enable])

dnl check for tools (compiler etc.)
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_CXX

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

PKG_CHECK_MODULES(GNUTLS, [
  gnutls >= $GNUTLS_MIN_REQUIRED
], [
], [
  AC_MSG_ERROR([
      You need to install or upgrade gnutls.
      The minimum version required is $GNUTLS_MIN_REQUIRED (libgnutls28-dev).
  ])
])

AC_MSG_CHECKING(for certtool)
if test "x$CERTTOOL_GNUTLS" != "x"; then
  AC_MSG_RESULT([$CERTTOOL_GNUTLS (from environment)])
else
  AC_PATH_PROG(CERTTOOL_GNUTLS, [certtool], [certtool])
  AC_MSG_RESULT([$CERTTOOL_GNUTLS])
fi
if ! $CERTTOOL_GNUTLS --version 2>/dev/null >/dev/null; then
  AC_MSG_ERROR([$CERTTOOL_GNUTLS does not seem to work!])
fi
AC_SUBST(CERTTOOL_GNUTLS)

PKG_CHECK_MODULES(SOUP, [
  libsoup-2.4 >= $SOUP_REQUIRED
], [
  AC_SUBST(SOUP_CFLAGS)
  AC_SUBST(SOUP_LIBS)
], [
  AC_MSG_ERROR([
    Lib soup not found.
  ])
])

AC_CHECK_LIB([sctp], [sctp_sendmsg],sctp_found=1,sctp_found=0)
if test "$sctp_found" = 1; then
  SCTP_LIBS="-lsctp"
  AC_SUBST(SCTP_LIBS)
else
  AC_MSG_ERROR(Required library sctp not found; use LDFLAGS to specify library location)
fi

dnl *** vpx ***
PKG_CHECK_MODULES(VPX, vpx >= 1.1.0, [HAVE_VPX="yes"], [HAVE_VPX="no"])
HAVE_VP8=no

if test $HAVE_VPX = "yes"; then
  OLD_LIBS="$LIBS"
  OLD_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $VPX_CFLAGS"
  LIBS="$LIBS $VPX_LIBS"
  AC_CHECK_LIB(vpx, vpx_codec_peek_stream_info, [
    HAVE_VP8=yes
    AC_DEFINE(HAVE_VP8_STREAM_INFO, 1, [Defined if the VP8 peek_stream_info is available])
  ], [
    AC_MSG_ERROR([
      VPX library with vpx_codec_peek_stream_info not found not found (package libvpx-dev)
    ])
  ])
  LIBS="$OLD_LIBS"
  CFLAGS="$OLD_CFLAGS"
else
  AC_MSG_ERROR([
    VPX library with vpx_codec_peek_stream_info not found not found (package libvpx-dev)
  ])
fi
AC_SUBST(VPX_LIBS)
AC_SUBST(VPX_CFLAGS)

dnl *** opencv ***
PKG_CHECK_MODULES(OPENCV, opencv >= 2.0.0 opencv <= 2.4.5 , [
  AC_CHECK_HEADERS([highgui.h opencv2/highgui/highgui_c.h])
  AC_LANG([C])

  if test $ac_cv_header_highgui_h = "yes" -o $ac_cv_header_opencv2_highgui_highgui_c_h = "yes" ; then
    AC_MSG_RESULT([Opencv correctly detected])
  else
    AC_MSG_ERROR([neither highgui.h nor opencv2/highgui/highgui_c.h could not be found, opencv needs to be installed])
  fi
])

dnl *** tesseract ***
AC_CHECK_LIB([tesseract], [TessBaseAPICreate],
  [
    AC_CHECK_HEADER([tesseract/capi.h],
      [
      ],
      [
        AC_MSG_ERROR([Could not find tesseract library])
      ]
    )
  ], [
    AC_MSG_ERROR([Could not find tesseract library])
  ]
)

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
  plugindir="$HOME/.gst-kurento-plugins/plugins"
else
  plugindir="\$(libdir)/gst-kurento-plugins"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

dnl FAILED TESTS

dnl enable/disable integration test
AC_ARG_ENABLE([integrationtests],
[  --enable-integrationtests    Turn on integration tests],
[case "${enableval}" in
  yes) integrationtests=true ;;
  no)  integrationtests=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-integrationtests]) ;;
esac],[integrationtest=false])
AM_CONDITIONAL([INTEGRATIONTESTS], [test x$integrationtests = xtrue])

dnl enable/disable automuxer test
AC_ARG_ENABLE([automuxertest],
[  --enable-automuxertest    Turn on testing automuxer],
[case "${enableval}" in
  yes) automuxertest=true ;;
  no)  automuxertest=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-automuxertest]) ;;
esac],[automuxertest=false])
AM_CONDITIONAL([AUTOMUXERTEST], [test x$automuxertest = xtrue])

dnl enable/disable agnosticbin test
AC_ARG_ENABLE([agnosticbin-test],
[  --enable-agnosticbin-test    Turn on agnosticbin tests],
[case "${enableval}" in
  yes) agnosticbintest=true ;;
  no)  agnosticbintest=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-agnosticbin-test]) ;;
esac],[agnosticbin=false])
AM_CONDITIONAL([AGNOSTICBINTEST], [test x$agnosticbintest = xtrue])

dnl enable/disable debuggingtests
AC_ARG_ENABLE([debugging-tests],
[  --enable-debugging-tests Turn on debugging tests],
[case "${enableval}" in
  yes) debuggingtests=true ;;
  no)  debuggingtests=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-debugging-tests]) ;;
esac],[debuggingtests=false])
AM_CONDITIONAL([DEBUGGING_TESTS], [test x$debuggingtests = xtrue])

if test "x${FINAL_INSTALL_DIR}" = "x"; then
  AC_DEFINE([FINAL_INSTALL_DIR], [""],"Final installation Path")
else
  AC_DEFINE_UNQUOTED([FINAL_INSTALL_DIR], ["$FINAL_INSTALL_DIR"],"Final installation Path")
fi

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/filters/Makefile
                 tests/Makefile
                 tests/check/Makefile
                 tests/examples/Makefile
                 src/filters/platedetector/Makefile
                 src/filters/jackvader/Makefile
                 src/filters/movementdetector/Makefile
                 src/filters/h264filter/Makefile
                 src/vp8parse/Makefile
                 src/filters/facedetector/Makefile
                 src/filters/imageoverlay/Makefile
                 src/filters/faceoverlay/Makefile
                 src/filters/chroma/Makefile
                 src/filters/crowddetector/Makefile
                 src/filters/pointerdetector2/Makefile])

AC_CONFIG_SUBDIRS([kms-dtls-plugins])
AC_OUTPUT
