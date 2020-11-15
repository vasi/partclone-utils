/*
 * libimage.c	- Image library.
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
#include <stdlib.h>
#include <errno.h>
#include "libimage.h"
#include "libntfsclone.h"
#include "libpartclone.h"
#include "librawimage.h"

extern image_dispatch_t partclone_image_type;
extern image_dispatch_t ntfsclone_image_type;
extern image_dispatch_t raw_image_type;

static const image_dispatch_t *known_types[] = {
    &ntfsclone_image_type,
    &partclone_image_type,
    &raw_image_type,		/* must be last */
};

#undef IMAGE_MAGIC
#define	IMAGE_MAGIC	0xceebee00
typedef struct image_handle {
    image_dispatch_t	*i_dispatch;
    sysdep_dispatch_t	*i_sysdep;
    void		*i_type_handle;
    u_int32_t		i_magic;
} image_handle_t;

int 
image_open(const char *path, const char *cfpath, 
	   sysdep_open_mode_t omode, const sysdep_dispatch_t *sysdep,
	   int raw_allowed, void **rpp)
{
    int itidx;
    int error = ENOENT;
    image_dispatch_t *fentry = (image_dispatch_t *) NULL;

    for (itidx = 0; itidx < (sizeof(known_types)/sizeof(known_types[0]));
	 itidx++) {
	if (!(error = (*known_types[itidx]->probe)(path, sysdep))) {
	    fentry = (image_dispatch_t *) known_types[itidx];
	    break;
	}
    }
    if (fentry && ((itidx < (sizeof(known_types)/sizeof(known_types[0]))-1) ||
		   raw_allowed)) {
	if (!(error = (*sysdep->sys_malloc)(rpp, sizeof(image_handle_t)))) {
	    image_handle_t *ihp = (image_handle_t *) *rpp;
	    ihp->i_magic = IMAGE_MAGIC;
	    ihp->i_sysdep = (sysdep_dispatch_t *) sysdep;
	    ihp->i_dispatch = (image_dispatch_t *) fentry;
	    error = (*ihp->i_dispatch->open)(path, cfpath, omode, sysdep,
					     &ihp->i_type_handle);
	}
    }
    return(error);
}

int 
image_close(void *rp)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    int error = EINVAL;
    if (ihp && (ihp->i_magic == IMAGE_MAGIC)) {
	error = (*ihp->i_dispatch->close)(ihp->i_type_handle);
	ihp->i_magic = 0;
	(void) (ihp->i_sysdep->sys_free)(ihp);
    } else {
      error = ESTALE;
    }
    return(error);
}

void
image_tolerant_mode(void *rp)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    if (ihp && (ihp->i_magic == IMAGE_MAGIC)) {
	(*ihp->i_dispatch->tolerant_mode)(ihp->i_type_handle);
    }
}

int
image_verify(void *rp)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    int error = EINVAL;
    if (ihp && (ihp->i_magic == IMAGE_MAGIC)) {
	error = (*ihp->i_dispatch->verify)(ihp->i_type_handle);
    }
    return(error);
}

int64_t
image_blocksize(void *rp)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    return((ihp && (ihp->i_magic == IMAGE_MAGIC)) ?
	   (*ihp->i_dispatch->blocksize)(ihp->i_type_handle) : -1);
}

int64_t
image_blockcount(void *rp)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    return((ihp && (ihp->i_magic == IMAGE_MAGIC)) ?
	   (*ihp->i_dispatch->blockcount)(ihp->i_type_handle) : -1);
}

int
image_seek(void *rp, u_int64_t blockno)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    int error = EINVAL;
    if (ihp && (ihp->i_magic == IMAGE_MAGIC)) {
	error = (*ihp->i_dispatch->seek)(ihp->i_type_handle, blockno);
    }
    return(error);
}

u_int64_t
image_tell(void *rp)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    return((ihp && (ihp->i_magic == IMAGE_MAGIC)) ?
	   (*ihp->i_dispatch->tell)(ihp->i_type_handle) : ~0);
}

int
image_readblocks(void *rp, void *buffer, u_int64_t nblocks)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    int error = EINVAL;
    if (ihp && (ihp->i_magic == IMAGE_MAGIC)) {
	error = (*ihp->i_dispatch->readblocks)(ihp->i_type_handle, 
					       buffer, nblocks);
    }
    return(error);
}

int
image_block_used(void *rp)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    return((ihp && (ihp->i_magic == IMAGE_MAGIC)) ?
	   (*ihp->i_dispatch->block_used)(ihp->i_type_handle) : BLOCK_ERROR);
}

int
image_writeblocks(void *rp, void *buffer, u_int64_t nblocks)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    int error = EINVAL;
    if (ihp && (ihp->i_magic == IMAGE_MAGIC)) {
	error = (*ihp->i_dispatch->writeblocks)(ihp->i_type_handle, 
						buffer, nblocks);
    }
    return(error);
}

int
image_sync(void *rp)
{
    image_handle_t *ihp = (image_handle_t *) rp;
    int error = EINVAL;
    if (ihp && (ihp->i_magic == IMAGE_MAGIC)) {
	error = (*ihp->i_dispatch->sync)(ihp->i_type_handle);
    }
    return(error);
}
