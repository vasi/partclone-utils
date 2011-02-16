#
# Makefile.am	- automake input for src
#
# @(#) $RCSfile: Makefile.am,v $ $Revision: 1.2 $ (Ideal World, Inc.) $Date: 2010/07/18 15:31:00 $
#
# Copyright (c) 2010, Ideal World, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
sbin_PROGRAMS = imagemount
noinst_PROGRAMS = libpctest

noinst_HEADERS = sysdep_int.h sysdep_posix.h partclone.h libpartclone.h
noinst_LIBRARIES = libpartclone.a libsysdep_posix.a
libpartclone_a_SOURCES = libpartclone.c
libsysdep_posix_a_SOURCES = sysdep_posix.c

imagemount_SOURCES = imagemount.c
imagemount_LDADD = libpartclone.a libsysdep_posix.a -lcap
libpctest_SOURCES = libpctest.c
libpctest_LDADD = libpartclone.a libsysdep_posix.a
