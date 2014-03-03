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
sbin_PROGRAMS = imagemount partclone_imageinfo ntfsclone_imageinfo
noinst_PROGRAMS = libpctest libntfstest cfdump

noinst_HEADERS = sysdep_int.h sysdep_posix.h partclone.h libpartclone.h libntfsclone.h libimage.h changefile.h changefileint.h ntfsclone.h librawimage.h
noinst_LIBRARIES = librawimage.a libntfsclone.a libpartclone.a libimage.a libchangefile.a libsysdep_posix.a
librawimage_a_SOURCES = librawimage.c
libntfsclone_a_SOURCES = libntfsclone.c
libpartclone_a_SOURCES = libpartclone.c
libimage_a_SOURCES = libimage.c
libchangefile_a_SOURCES = changefile.c
libsysdep_posix_a_SOURCES = sysdep_posix.c

imagemount_SOURCES = imagemount.c
imagemount_LDADD = libimage.a libpartclone.a libntfsclone.a librawimage.a libchangefile.a libsysdep_posix.a 
libpctest_SOURCES = libpctest.c
libpctest_LDADD = libpartclone.a libchangefile.a libsysdep_posix.a
libntfstest_SOURCES = libntfstest.c
libntfstest_LDADD = libntfsclone.a libchangefile.a libsysdep_posix.a
partclone_imageinfo_SOURCES = partclone_imageinfo.c
partclone_imageinfo_LDADD = libpartclone.a libchangefile.a libsysdep_posix.a
ntfsclone_imageinfo_SOURCES = ntfsclone_imageinfo.c
ntfsclone_imageinfo_LDADD = libntfsclone.a libchangefile.a libsysdep_posix.a
cfdump_SOURCES = cfdump.c
cfdump_LDADD = libchangefile.a libsysdep_posix.a
