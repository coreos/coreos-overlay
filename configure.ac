#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([update_engine],[0.4.2],[https://github.com/coreos/bugs/issues])
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

AC_CHECK_LIB([rootdev], [rootdev], [],
             [AC_MSG_ERROR([*** librootdev not found])])
AC_CHECK_HEADERS([rootdev/rootdev.h], [],
                 [AC_MSG_ERROR([*** rootdev.h not found])])

# Detect libgflags presence and namespace. (may be gflags or google)
# autoconf's C++ mode is only enabled for this test because other
# macros like AM_PATH_GLIB_2_0 assume the default C mode.
LIBS="$LIBS -lgflags"
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS([gflags/gflags.h], [],
                 [AC_MSG_ERROR([*** gflags/gflags.h not found])])
AC_CACHE_CHECK(
 [gflags namespace],
 [ac_cv_gflags_namespace],
 [ac_cv_gflags_namespace="no"
  # gflags prior to 2.1.2 does not define GFLAGS_NAMESPACE
  AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <gflags/gflags.h>
                     #ifndef GFLAGS_NAMESPACE
                     #error GFLAGS_NAMESPACE
                     #endif]])],
                    [try_gflags_namespaces=GFLAGS_NAMESPACE],
                    [try_gflags_namespaces="gflags google"])
  for try_gflags_ns in $try_gflags_namespaces
  do
    AC_LINK_IFELSE([AC_LANG_SOURCE([[#include <gflags/gflags.h>
      int main(int argc, char** argv) {
        return ${try_gflags_ns}::ParseCommandLineFlags(&argc, &argv, true);
      }]])],
      [ac_cv_gflags_namespace="$try_gflags_ns"; break])
  done])
AC_LANG_POP([C++])
AS_IF([test "x${ac_cv_gflags_namespace}" = "xno"],
      [AC_MSG_ERROR([*** gflags not found])])
AS_IF([test "x${ac_cv_gflags_namespace}" != "xGFLAGS_NAMESPACE"],
      [AC_DEFINE_UNQUOTED([GFLAGS_NAMESPACE], [$ac_cv_gflags_namespace])])

AM_PATH_GLIB_2_0([2.36.0], [],
                 [AC_MSG_ERROR([*** glib >= 2.36 not found])],
                 gthread gio gio-unix)

PKG_CHECK_MODULES([DEPS],
                  [dbus-1
                   dbus-glib-1
                   ext2fs
                   libcrypto
                   libcurl
                   libglog
                   libssl
                   libxml-2.0
                   protobuf])

# Make sure logging can be configured with command line flags. If glog
# was built with gflags support it will include the gflags header.
AC_MSG_CHECKING([glog uses gflags])
AC_LANG_PUSH([C++])
AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <glog/logging.h>
                                    #ifndef DECLARE_VARIABLE
                                    #error MISSING_GFLAGS
                                    #endif]])],
                  [AC_MSG_RESULT([yes])],
                  [AC_MSG_RESULT([no])
                   AC_MSG_ERROR([*** glog missing gflags support])])
AC_LANG_POP([C++])

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
