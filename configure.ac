#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([update_engine],[0.1.0],[https://github.com/coreos/bugs/issues])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([src/update_engine/main.cc])
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

AM_INIT_AUTOMAKE([foreign 1.13 -Wall -Wno-portability
                  serial-tests silent-rules subdir-objects])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_MKDIR_P
AC_PROG_RANLIB
AM_PROG_AR

AC_PATH_PROG([DBUS_BINDING_TOOL], [dbus-binding-tool])
AS_IF([test "x${DBUS_BINDING_TOOL}" = "x"],
      [AC_MSG_ERROR([*** dbus-binding-tool not found])])

AC_PATH_PROG([PROTOC], [protoc])
AS_IF([test "x${PROTOC}" = "x"],
      [AC_MSG_ERROR([*** protoc not found])])

# Checks for libraries.
AC_CHECK_LIB([bz2], [BZ2_bzDecompressInit], [],
             [AC_MSG_ERROR([*** libbz2 not found])])
AC_CHECK_HEADERS([bzlib.h], [],
                 [AC_MSG_ERROR([*** bzlib.h not found])])

AM_PATH_GLIB_2_0([2.36.0], [],
                 [AC_MSG_ERROR([*** glib >= 2.36 not found])],
                 gthread gio gio-unix)

PKG_CHECK_MODULES([DEPS],
                  [dbus-1
                   dbus-glib-1
                   ext2fs
                   libchrome-180609
                   libcrypto
                   libcurl
                   libgflags
                   libssl
                   libxml-2.0
                   protobuf])

AC_ARG_ENABLE([delta_generator],
              [AS_HELP_STRING([--enable-delta_generator],
                              [build delta_generator])],
              [], [enable_delta_generator=yes])
AM_CONDITIONAL([ENABLE_DELTA_GENERATOR],
               [test "x$enable_delta_generator" = "xyes"])

AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug],
                              [build with debug options])],
              [], [enable_debug=yes])
AM_CONDITIONAL([ENABLE_DEBUG], [test "x$enable_debug" = "xyes"])

AC_ARG_ENABLE([gcov],
              [AS_HELP_STRING([--enable-gcov],
                              [use gcov to gather test coverage data])])
AM_CONDITIONAL([ENABLE_GCOV], [test "x$enable_gcov" = "xyes"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
