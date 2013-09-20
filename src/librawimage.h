/*
 * librawimage.h	- Interfaces to raw image library.
 */
/*
 * Copyright (c) 2014, Ideal World, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#ifndef	_LIBRAWIMAGE_H_
#define	_LIBRAWIMAGE_H_	1
#include <sys/types.h>
#include "sysdep_int.h"
int rawimage_open(const char *path, const char *cfpath, 
		   sysdep_open_mode_t omode, const sysdep_dispatch_t *sysdep,
		   void **rpp);
int rawimage_close(void *rp);
int rawimage_verify(void *rp);
int64_t rawimage_blocksize(void *rp);
int64_t rawimage_blockcount(void *rp);
int rawimage_seek(void *rp, u_int64_t blockno);
u_int64_t rawimage_tell(void *rp);
int rawimage_readblocks(void *rp, void *buffer, u_int64_t nblocks);
int rawimage_block_used(void *rp);
int rawimage_writeblocks(void *rp, void *buffer, u_int64_t nblocks);
int rawimage_sync(void *rp);

#endif	/* _LIBRAWIMAGE_H_ */
