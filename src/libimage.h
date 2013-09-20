/*
 * libimage.h	- Interfaces to image library.
 */
/*
 * Copyright (c) 2011, Ideal World, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#ifndef	_LIBIMAGE_H_
#define	_LIBIMAGE_H_	1
#include <sys/types.h>
#include "sysdep_int.h"

#define	BLOCK_ERROR	-2

/*
 * Per-image type dispatch table.
 */
typedef struct image_type_dispatch {
    const char *type_name;
    int (*probe)(const char *path, const sysdep_dispatch_t *sysdep);
    int (*open)(const char *path, const char *cfpath, 
		sysdep_open_mode_t omode, const sysdep_dispatch_t *sysdep,
		void **rpp);
    int (*close)(void *rp);
    void (*tolerant_mode)(void *rp);
    int (*verify)(void *rp);
    int64_t (*blocksize)(void *rp);
    int64_t (*blockcount)(void *rp);
    int (*seek)(void *rp, u_int64_t blockno);
    u_int64_t (*tell)(void *rp);
    int (*readblocks)(void *rp, void *buffer, u_int64_t nblocks);
    int (*block_used)(void *rp);
    int (*writeblocks)(void *rp, void *buffer, u_int64_t nblocks);
    int (*sync)(void *rp);
} image_dispatch_t;

/*
 * Our interface to everybody else.
 */
int image_open(const char *path, const char *cfpath, 
	       sysdep_open_mode_t omode, const sysdep_dispatch_t *sysdep,
	       int raw_allowed, void **rpp);
int image_close(void *rp);
void image_tolerant_mode(void *rp);
int image_verify(void *rp);
int64_t image_blocksize(void *rp);
int64_t image_blockcount(void *rp);
int image_seek(void *rp, u_int64_t blockno);
u_int64_t image_tell(void *rp);
int image_readblocks(void *rp, void *buffer, u_int64_t nblocks);
int image_block_used(void *rp);
int image_writeblocks(void *rp, void *buffer, u_int64_t nblocks);
int image_sync(void *rp);

#endif	/* _LIBIMAGE_H_ */
