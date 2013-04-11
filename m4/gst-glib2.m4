dnl check for a minimum version of GLib

dnl AG_GST_GLIB_CHECK([minimum-version-required])

AC_DEFUN([AG_GST_GLIB_CHECK],
[
  #AC_REQUIRE([AS_NANO])

  dnl Minimum required version of GLib
  GLIB_REQ=[$1]
  if test "x$GLIB_REQ" = "x"
  then
    AC_MSG_ERROR([Please specify a required version for GLib 2.0])
  fi
  AC_SUBST(GLIB_REQ)

  dnl Check for glib with everything
  AG_GST_PKG_CHECK_MODULES(GLIB,
    glib-2.0 >= $GLIB_REQ gobject-2.0 gthread-2.0 gmodule-no-export-2.0)

  if test "x$HAVE_GLIB" = "xno"; then
    AC_MSG_ERROR([This package requires GLib >= $GLIB_REQ to compile.])
  fi

  dnl Add define to tell GLib that threading is always enabled within GStreamer
  dnl code (optimisation, bypasses checks if the threading system is enabled
  dnl when using threading primitives)
  GLIB_EXTRA_CFLAGS="$GLIB_EXTRA_CFLAGS -DG_THREADS_MANDATORY"

  dnl Define G_DISABLE_DEPRECATED for GIT versions
  if test "x$PACKAGE_VERSION_NANO" = "x1"; then
    GLIB_EXTRA_CFLAGS="$GLIB_EXTRA_CFLAGS -DG_DISABLE_DEPRECATED"
  fi

  AC_ARG_ENABLE(gobject-cast-checks,
    AS_HELP_STRING([--enable-gobject-cast-checks[=@<:@no/auto/yes@:>@]],
      [Enable GObject cast checks]),,
    [enable_gobject_cast_checks=auto])

  if test "x$enable_gobject_cast_checks" = "xauto"; then
    dnl For releases, turn off the cast checks
    if test "x$PACKAGE_VERSION_NANO" = "x1"; then
      enable_gobject_cast_checks=yes
    else
      enable_gobject_cast_checks=no
    fi
  fi

  if test "x$enable_gobject_cast_checks" = "xno"; then
    GLIB_EXTRA_CFLAGS="$GLIB_EXTRA_CFLAGS -DG_DISABLE_CAST_CHECKS"
  fi

  AC_ARG_ENABLE(glib-asserts,
    AS_HELP_STRING([--enable-glib-asserts[=@<:@no/auto/yes@:>@]],
      [Enable GLib assertion]),,
    [enable_glib_assertions=auto])

  if test "x$enable_glib_assertions" = "xauto"; then
    dnl For releases, turn off the assertions
    if test "x$PACKAGE_VERSION_NANO" = "x1"; then
      enable_glib_assertions=yes
    else
      enable_glib_assertions=no
    fi
  fi

  if test "x$enable_glib_assertions" = "xno"; then
    GLIB_EXTRA_CFLAGS="$GLIB_EXTRA_CFLAGS -DG_DISABLE_ASSERT"
  fi

  dnl Find location of glib utils. People may want to or have to override these,
  dnl e.g. in a cross-compile situation where PATH is a bit messed up. We need
  dnl for these tools to work on the host, so can't just use the one from the
  dnl GLib installation that pkg-config picks up, as that might be for a
  dnl different target architecture.
  dnl
  dnl glib-genmarshal:
  AC_MSG_CHECKING(for glib-genmarshal)
  if test "x$GLIB_GENMARSHAL" != "x"; then
    AC_MSG_RESULT([$GLIB_GENMARSHAL (from environment)])
  else
    GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
    if $GLIB_GENMARSHAL --version 2>/dev/null >/dev/null; then
      AC_MSG_RESULT([$GLIB_GENMARSHAL (from pkg-config path)])
    else
      AC_PATH_PROG(GLIB_GENMARSHAL, [glib-genmarshal], [glib-genmarshal])
      AC_MSG_RESULT([$GLIB_GENMARSHAL])
    fi
  fi
  if ! $GLIB_GENMARSHAL --version 2>/dev/null >/dev/null; then
    AC_MSG_WARN([$GLIB_GENMARSHAL does not seem to work!])
  fi
  AC_SUBST(GLIB_GENMARSHAL)

  dnl glib-mkenums:
  AC_MSG_CHECKING(for glib-mkenums)
  if test "x$GLIB_MKENUMS" != "x"; then
    AC_MSG_RESULT([$GLIB_MKENUMS (from environment)])
  else
    dnl glib-mkenums is written in perl so should always work really
    GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
    AC_MSG_RESULT([$GLIB_MKENUMS])
  fi
  if ! $GLIB_MKENUMS --version 2>/dev/null >/dev/null; then
    AC_MSG_WARN([$GLIB_MKENUMS does not seem to work!])
  fi
  AC_SUBST(GLIB_MKENUMS)

  dnl for the poor souls who for example have glib in /usr/local
  AS_SCRUB_INCLUDE(GLIB_CFLAGS)

  AC_SUBST(GLIB_EXTRA_CFLAGS)

  dnl Now check for GIO
  PKG_CHECK_MODULES(GIO, gio-2.0 >= $GLIB_REQ)
  if test "x$HAVE_GIO" = "xno"; then
    AC_MSG_ERROR([This package requires GIO >= $GLIB_REQ to compile.])
  fi

  GIO_MODULE_DIR="`$PKG_CONFIG --variable=giomoduledir gio-2.0`"
  AC_DEFINE_UNQUOTED(GIO_MODULE_DIR, "$GIO_MODULE_DIR",
      [The GIO modules directory.])
  GIO_LIBDIR="`$PKG_CONFIG --variable=libdir gio-2.0`"
  AC_DEFINE_UNQUOTED(GIO_LIBDIR, "$GIO_LIBDIR",
      [The GIO library directory.])
  AC_SUBST(GIO_CFLAGS)
  AC_SUBST(GIO_LIBS)
  AC_SUBST(GIO_LDFLAGS)
])
