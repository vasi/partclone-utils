#
# configure.ac	- autoconf definitions for partclone-utils
#
# @(#) $RCSfile: configure.ac,v $ $Revision: 1.4 $ (Ideal World, Inc.) $Date: 2010/08/15 12:40:41 $
#
# Copyright (c) 2010, Ideal World, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#

# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.62)
AC_INIT([partclone-utils], [0.2.2], [xppppp@sf.net])
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_RANLIB
AM_PROG_AR

# Checks for libraries.
AC_CHECK_LIB([cap], [cap_init])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/ioctl.h sys/mount.h sys/socket.h syslog.h unistd.h sys/capability.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strerror])

AC_SYS_LARGEFILE
AC_MSG_CHECKING( [whether _LARGEFILE64_SOURCE is needed] )
if test "$ac_cv_sys_file_offset_bits" != no \
     -o "$ac_cv_sys_large_files" != no ; then
    AC_DEFINE( _LARGEFILE64_SOURCE, 1,
               [Define if 64-bit file address support in 32-bit OS.] )
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

AC_ARG_WITH([rundir],
	[AS_HELP_STRING([--with-rundir],
	[Place pid file in this directory (default=/var/run)])],	
	[rundir=$with_rundir],
	[rundir=/var/run])

AC_DEFINE_UNQUOTED([RUNDIR], ["$rundir"], [Place pid file in this directory])

AC_CONFIG_FILES([Makefile
                 src/Makefile
		 docs/Makefile
		 docs/imagemount.8])
AC_OUTPUT
