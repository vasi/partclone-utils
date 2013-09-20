/*
 * librawimage.c	- Access individual blocks in a raw image.
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
#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */
#include <errno.h>
#include <string.h>
#include "changefile.h"
#include "librawimage.h"
#include "libimage.h"

static const char cf_trailer[] = ".cf";

/*
 * raw_context_t	- Handle to access rawimage images.  Used internally.
 */
struct change_file_context;
#define	RAW_OPEN	0x0001		/* Image is open. */
#define	RAW_CF_OPEN	0x0002		/* Change file is open */
#define	RAW_VERIFIED	0x0004		/* Image verified */
#define	RAW_HAVE_CFDEP	0x0040		/* Image has change file handle */
#define	RAW_CF_VERIFIED	0x0200		/* Change file verified. */
#define	RAW_CF_INIT	0x0400		/* Change file init done. */
#define	RAW_HAVE_PATH	0x2000		/* Path string allocated */
#define	RAW_HAVE_CF_PATH 0x4000		/* Path string allocated */
#define	RAW_VALID	0x8000		/* Header is valid */
#define	RAW_TOLERANT	0x40000		/* Open in tolerant mode */
#define	RAW_READ_ONLY	0x80000		/* Open read only */
typedef struct libraw_context {
    void		*raw_fd;	/* File handle */
    char 		*raw_path;	/* Path to image */
    char		*raw_cf_path;	/* Path to change file */
    void		*raw_cf_handle;	/* Change file handle */
    const sysdep_dispatch_t
			*raw_sysdep;	/* System-specific routines */
    u_int64_t		raw_blocksize;	/* block size */
    u_int64_t		raw_totalblocks; /* total number of blocks */
    u_int64_t		raw_curblock;	/* Current position */
    u_int32_t		raw_flags;	/* Handle flags */
    sysdep_open_mode_t	raw_omode;	/* Open mode */
} raw_context_t;

/*
 * Macros to check state flags.
 */
#define	RAWCTX_FLAGS_SET(_p, _f)	((_p) && (((_p)->raw_flags & ((_f)|RAW_VALID)) \
					  == ((_f)|RAW_VALID)))
#define	RAWCTX_VALID(_p)	RAWCTX_FLAGS_SET(_p, 0)
#define	RAWCTX_OPEN(_p)		RAWCTX_FLAGS_SET(_p, RAW_OPEN)
#define	RAWCTX_TOLERANT(_p)	RAWCTX_FLAGS_SET(_p, RAW_TOLERANT)
#define	RAWCTX_READ_ONLY(_p)	(((_p)->raw_flags & RAW_READ_ONLY) == \
				 RAW_READ_ONLY)
#define	RAWCTX_CF_OPEN(_p)	RAWCTX_FLAGS_SET(_p, RAW_CF_OPEN)
#define	RAWCTX_VERIFIED(_p)	RAWCTX_FLAGS_SET(_p, RAW_OPEN|RAW_VERIFIED)
#define	RAWCTX_READREADY(_p)	RAWCTX_FLAGS_SET(_p, RAW_OPEN|RAW_VERIFIED)
#define	RAWCTX_CFREADY(_p)	RAWCTX_FLAGS_SET(_p, RAW_OPEN|RAW_VERIFIED| \
					       RAW_HAVE_CFDEP|RAW_CF_VERIFIED)
#define	RAWCTX_WRITEABLE(_p)	(!RAWCTX_READ_ONLY(_p) && RAWCTX_READREADY(_p))
#define	RAWCTX_WRITEREADY(_p)	(!RAWCTX_READ_ONLY(_p) && RAWCTX_CFREADY(_p))
#define	RAWCTX_HAVE_PATH(_p)	(RAWCTX_FLAGS_SET(_p, RAW_HAVE_PATH) && \
				 (_p)->raw_path)
#define	RAWCTX_HAVE_CF_PATH(_p)	(RAWCTX_FLAGS_SET(_p, RAW_HAVE_CF_PATH) && \
				 (_p)->raw_cf_path)
#define	RAWCTX_HAVE_CFDEP(_p)	(RAWCTX_FLAGS_SET(_p, RAW_HAVE_CFDEP) && \
				 (_p)->raw_cfdep)

#define	RAW_BLOCKSIZE	512

/*
 * rblock2offset	- Calculate offset in image file of particular
 *			  block.
 */
static inline int64_t
rblock2offset(raw_context_t *rcp, u_int64_t rbnum)
{
    return(rbnum * rcp->raw_blocksize);
}

/*
 * rawimage_close()	- Close the image handle.
 */
int
rawimage_close(void *rp)
{
    int error = EINVAL;
    raw_context_t *rcp = (raw_context_t *) rp;

    if (RAWCTX_VALID(rcp)) {
	if (RAWCTX_OPEN(rcp)) {
	    (void) (*rcp->raw_sysdep->sys_close)(rcp->raw_fd);
	}
	if (RAWCTX_HAVE_PATH(rcp)) {
	    (void) (*rcp->raw_sysdep->sys_free)(rcp->raw_path);
	}
	if (RAWCTX_HAVE_CF_PATH(rcp)) {
	    (void) (*rcp->raw_sysdep->sys_free)(rcp->raw_cf_path);
	}
	if (RAWCTX_CF_OPEN(rcp)) {
	    (void) cf_sync(rcp->raw_cf_handle);
	    (void) cf_finish(rcp->raw_cf_handle);
	}
	(void) (*rcp->raw_sysdep->sys_free)(rcp);
	error = 0;
    }
    return(error);
}

/*
 * rawimage_open	- Open an image handle using the system-specific
 *			  interfaces.
 */
int
rawimage_open(const char *path, const char *cfpath, sysdep_open_mode_t omode,
	       const sysdep_dispatch_t *sysdep, void **rpp)
{
    int error = EINVAL;
    if (sysdep) {
	raw_context_t *rcp;
	error = (*sysdep->sys_malloc)(&rcp, sizeof(*rcp));

	if (rcp) {
	    memset(rcp, 0, sizeof(*rcp));
	    rcp->raw_blocksize = RAW_BLOCKSIZE;
	    rcp->raw_flags |= RAW_VALID;
	    rcp->raw_sysdep = sysdep;

	    if ((error = (*rcp->raw_sysdep->sys_open)(&rcp->raw_fd,
						     path,
						     SYSDEP_OPEN_RO)) == 0) {
		u_int64_t filesize;
		if ((error = (*rcp->raw_sysdep->sys_file_size)(rcp->raw_fd,
							       &filesize)
			) == 0) {
		    if (filesize > 100000000000) {
			rcp->raw_blocksize = 4096;
		    }
		    rcp->raw_totalblocks = filesize / rcp->raw_blocksize;
		    rcp->raw_flags |= RAW_OPEN;
		    if ((error = 
			 (*rcp->raw_sysdep->sys_malloc)(&rcp->raw_path,
							strlen(path)+1)) == 0) {
			rcp->raw_flags |= RAW_HAVE_PATH;
			rcp->raw_omode = omode;
			if (cfpath &&
			    ((error = 
			      (*rcp->raw_sysdep->sys_malloc)(&rcp->raw_cf_path,
							     strlen(cfpath)+1)) 
			     == 0)) {
			    rcp->raw_flags |= RAW_HAVE_CF_PATH;
			    memcpy(rcp->raw_cf_path, cfpath, strlen(cfpath)+1);
			}
			if (!error)
			    *rpp = (void *) rcp;
		    }
		}
	    }
	    if (error) {
		rawimage_close(rcp);
	    }
	}
    }
    if (error) {
	*rpp = (void *) NULL;
    }
    return(error);
}

/*
 * rawimage_tolerant_mode	- Set tolerant mode (does nothing)
 */
void
rawimage_tolerant_mode(void *rp)
{
    raw_context_t *rcp = (raw_context_t *) rp;

    if (RAWCTX_OPEN(rcp)) {
	rcp->raw_flags |= RAW_TOLERANT;
    }
}

/*
 * rawimage_verify	- Verify the image.
 */
int
rawimage_verify(void *rp)
{
    int error = EINVAL;
    raw_context_t *rcp = (raw_context_t *) rp;

    if (RAWCTX_OPEN(rcp)) {
	error = 0;
	if (rcp->raw_cf_path && 
	    ((int) rcp->raw_omode >= (int) SYSDEP_OPEN_RW)) {
	    if ((error = cf_init(rcp->raw_cf_path, rcp->raw_sysdep,
				 rcp->raw_blocksize,
				 rcp->raw_totalblocks,
				 &rcp->raw_cf_handle)) == 0) {
		rcp->raw_flags |= RAW_CF_OPEN;
		if ((error = cf_verify(rcp->raw_cf_handle)) == 0) {
		    rcp->raw_flags |= RAW_CF_VERIFIED;
		}
	    } else {
		/*
		 * We'll create this later.
		 */
		error = 0;
	    }
	} else {
	    if ((int) rcp->raw_omode < (int) SYSDEP_OPEN_RW)
		rcp->raw_flags |= RAW_READ_ONLY;
	}
	if (!error) {
	    rcp->raw_flags |= RAW_VERIFIED;
	}
    }
    return(error);
}

/*
 * rawimage_blocksize	- Return the blocksize.
 */
int64_t
rawimage_blocksize(void *rp)
{
    raw_context_t *rcp = (raw_context_t *) rp;
    return((RAWCTX_VERIFIED(rcp)) ? rcp->raw_blocksize : -1);
}

/*
 * rawimage_blockcount	- Return the total count of blocks.
 */
int64_t
rawimage_blockcount(void *rp)
{
    raw_context_t *rcp = (raw_context_t *) rp;
    return((RAWCTX_VERIFIED(rcp)) ? rcp->raw_totalblocks : -1);
}

/*
 * rawimage_seek	- Seek to a particular block.
 */
int
rawimage_seek(void *rp, u_int64_t blockno)
{
    int error = EINVAL;
    raw_context_t *rcp = (raw_context_t *) rp;

    if (RAWCTX_READREADY(rcp) && (blockno <= rcp->raw_totalblocks)) {
	rcp->raw_curblock = blockno;
	error = (rcp->raw_cf_handle) ? cf_seek(rcp->raw_cf_handle, blockno) : 0;
    }
    return(error);
}

/*
 * rawimage_tell	- Obtain the current position.
 */
u_int64_t
rawimage_tell(void *rp)
{
    raw_context_t *rcp = (raw_context_t *) rp;

    return((RAWCTX_READREADY(rcp)) ? rcp->raw_curblock : ~0);
}

/*
 * rawimage_readblocks	- Read blocks from the current position.
 */
int
rawimage_readblocks(void *rp, void *buffer, u_int64_t nblocks)
{
    int error = EINVAL;
    raw_context_t *rcp = (raw_context_t *) rp;
    if (RAWCTX_READREADY(rcp)) {
	u_int64_t nread;
	if (rcp->raw_cf_handle) {
	    u_int64_t bindex;
	    void *cbp = buffer;

	    for (bindex = 0; bindex < nblocks; bindex++) {
		cf_seek(rcp->raw_cf_handle, rcp->raw_curblock);
		if ((error = cf_readblock(rcp->raw_cf_handle, cbp))) {
		    if ((error == ENXIO) &&
			(error = (*rcp->raw_sysdep->sys_seek)
			 (rcp->raw_fd, rblock2offset(rcp, rcp->raw_curblock),
			  SYSDEP_SEEK_ABSOLUTE, (u_int64_t *) NULL)) == 0) {
			error = (*rcp->raw_sysdep->sys_read)
			    (rcp->raw_fd, cbp, rcp->raw_blocksize, &nread);
		    }
		    if (error) {
			break;
		    }
		}
		rcp->raw_curblock++;
		cbp += rcp->raw_blocksize;
	    }
	} else {
	    if ((error = (*rcp->raw_sysdep->sys_seek)
		 (rcp->raw_fd, rblock2offset(rcp, rcp->raw_curblock),
		  SYSDEP_SEEK_ABSOLUTE, (u_int64_t *) NULL)) == 0) {
		error = (*rcp->raw_sysdep->sys_read)
		    (rcp->raw_fd, buffer, nblocks * rcp->raw_blocksize, &nread);
	    }
	}
    }
    return(error);
}

/*
 * rawimage_block_used	- Determine if the current block is used.
 */
int
rawimage_block_used(void *rp)
{
    raw_context_t *rcp = (raw_context_t *) rp;
    return((RAWCTX_READREADY(rcp) && 
	    (rcp->raw_curblock < rcp->raw_totalblocks)) ? 1 : BLOCK_ERROR);
}

/*
 * rawimage_writeblocks	- Write blocks to the current position.
 */
int
rawimage_writeblocks(void *rp, void *buffer, u_int64_t nblocks)
{
    int error = EINVAL;
    raw_context_t *rcp = (raw_context_t *) rp;

    if (RAWCTX_WRITEABLE(rcp)) {
	/*
	 * Make sure we're initialized.
	 */
	if (!RAWCTX_WRITEREADY(rcp)) {
	    if (!RAWCTX_HAVE_CF_PATH(rcp)) {
		/*
		 * We have to make up a name.
		 */
		if ((error = (*rcp->raw_sysdep->sys_malloc)
		     (&rcp->raw_cf_path,
		      strlen(rcp->raw_path) +
		      strlen(cf_trailer) + 1)) 
		    == 0) {
		    memcpy(rcp->raw_cf_path, rcp->raw_path, 
			   strlen(rcp->raw_path));
		    memcpy(&rcp->raw_cf_path[strlen(rcp->raw_path)],
			   cf_trailer, strlen(cf_trailer)+1);
		    rcp->raw_flags |= RAW_HAVE_CF_PATH;
		}
	    }
	    error = cf_create(rcp->raw_cf_path, rcp->raw_sysdep,
			      rcp->raw_blocksize, rcp->raw_totalblocks,
			      &rcp->raw_cf_handle);
	    if (!error) {
		rcp->raw_flags |= (RAW_HAVE_CFDEP|RAW_CF_VERIFIED|RAW_CF_OPEN);
	    }
	} else {
	    error = 0;
	}
	if (!error) {
	    void *cbp = buffer;
	    u_int64_t bindex;

	    for (bindex = 0; bindex < nblocks; bindex++) {
		cf_seek(rcp->raw_cf_handle, rcp->raw_curblock);
		if ((error = cf_writeblock(rcp->raw_cf_handle, cbp))) {
		    break;
		}
		rcp->raw_curblock++;
		cbp += rcp->raw_blocksize;
	    }
	}
    }
    return(error);
}

/*
 * rawimage_sync	- Commit changes to image.
 */
int
rawimage_sync(void *rp)
{
    raw_context_t *rcp = (raw_context_t *) rp;

    return( (RAWCTX_WRITEREADY(rcp)) ?
	    cf_sync(rcp->raw_cf_handle) :
	    EINVAL );
}

/*
 * rawimage_probe	- Is this a rawimage image?
 */
int
rawimage_probe(const char *path, const sysdep_dispatch_t *sysdep)
{
    void *testh = (void *) NULL;
    int error = rawimage_open(path, (char *) NULL, SYSDEP_OPEN_RO,
			       sysdep, &testh);
    if (!error) {
	error = rawimage_verify(testh);
	rawimage_close(testh);
    }
    return(error);
}

/*
 * The image type dispatch table.
 */
const image_dispatch_t raw_image_type = {
    "raw image",
    rawimage_probe,
    rawimage_open,
    rawimage_close,
    rawimage_tolerant_mode,
    rawimage_verify,
    rawimage_blocksize,
    rawimage_blockcount,
    rawimage_seek,
    rawimage_tell,
    rawimage_readblocks,
    rawimage_block_used,
    rawimage_writeblocks,
    rawimage_sync
};
