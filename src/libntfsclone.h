/*
 * libntfsclone.h	- Interfaces to ntfsclone library.
 */
/*
 * @(#) $RCSfile: libntfsclone.h,v $ $Revision: 1.2 $ (Ideal World, Inc.) $Date: 2010/07/17 01:21:27 $
 */
/*
 * Copyright (c) 2010, Ideal World, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#ifndef	_LIBNTFSCLONE_H_
#define	_LIBNTFSCLONE_H_	1
#include <sys/types.h>
#include "sysdep_int.h"

int ntfsclone_open(const char *path, const char *cfpath, 
		   sysdep_open_mode_t omode, const sysdep_dispatch_t *sysdep,
		   void **rpp);
int ntfsclone_close(void *rp);
int ntfsclone_verify(void *rp);
int64_t ntfsclone_blocksize(void *rp);
int64_t ntfsclone_blockcount(void *rp);
int ntfsclone_seek(void *rp, u_int64_t blockno);
u_int64_t ntfsclone_tell(void *rp);
int ntfsclone_readblocks(void *rp, void *buffer, u_int64_t nblocks);
int ntfsclone_block_used(void *rp);
int ntfsclone_writeblocks(void *rp, void *buffer, u_int64_t nblocks);
int ntfsclone_sync(void *rp);

#endif	/* _LIBNTFSCLONE_H_ */
