/*
 * libntfsclone.c	- Access individual blocks in a ntfsclone image.
 */
/*
 * @(#) $RCSfile: libntfsclone.c,v $ $Revision: 1.4 $ (Ideal World, Inc.) $Date: 2010/07/17 20:47:58 $
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
#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */
#include <errno.h>
#include <string.h>
#include "changefile.h"
#include "ntfsclone.h"
#include "libntfsclone.h"
#include "libimage.h"

static const char cf_trailer[] = ".cf";

/*
 * nc_context_t	- Handle to access partclone images.  Used internally.
 */
struct version_dispatch_table;
struct change_file_context;
#define	NC_OPEN		0x0001		/* Image is open. */
#define	NC_CF_OPEN	0x0002		/* Change file is open */
#define	NC_VERIFIED	0x0004		/* Image verified */
#define	NC_HAVE_CFDEP	0x0040		/* Image has change file handle */
#define	NC_HAVE_VERDEP	0x0080		/* Image has version-dependent handle */
#define	NC_HAVE_IVBLOCK	0x0100		/* Image has invalid block. */
#define	NC_CF_VERIFIED	0x0200		/* Change file verified. */
#define	NC_CF_INIT	0x0400		/* Change file init done. */
#define	NC_VERSION_INIT	0x0800		/* Version-dependent init done. */
#define	NC_HEAD_VALID	0x1000		/* Image header valid. */
#define	NC_HAVE_PATH	0x2000		/* Path string allocated */
#define	NC_HAVE_CF_PATH	0x4000		/* Path string allocated */
#define	NC_VALID	0x8000		/* Header is valid */
#define	NC_TOLERANT	0x4000		/* Open in tolerant mode */
#define	NC_READ_ONLY	0x80000		/* Open read only */
typedef struct libntfsclone_context {
    void		*nc_fd;		/* File handle */
    char 		*nc_path;	/* Path to image */
    char		*nc_cf_path;	/* Path to change file */
    void		*nc_cf_handle;	/* Change file handle */
    unsigned char	*nc_ivblock;	/* Convenient invalid block */
    void		*nc_verdep;	/* Version-dependent handle */
    struct version_dispatch_table
			*nc_dispatch;	/* Version-dependent dispatch */
    const sysdep_dispatch_t
			*nc_sysdep;	/* System-specific routines */
    image_hdr		nc_head;	/* Image header */
    u_int64_t		nc_curblock;	/* Current position */
    u_int32_t		nc_flags;	/* Handle flags */
    sysdep_open_mode_t	nc_omode;	/* Open mode */
} nc_context_t;

/*
 * Macros to check state flags.
 */
#define	NTCTX_FLAGS_SET(_p, _f)	((_p) && (((_p)->nc_flags & ((_f)|NC_VALID)) \
					  == ((_f)|NC_VALID)))
#define	NTCTX_VALID(_p)		NTCTX_FLAGS_SET(_p, 0)
#define	NTCTX_OPEN(_p)		NTCTX_FLAGS_SET(_p, NC_OPEN)
#define	NTCTX_TOLERANT(_p)	NTCTX_FLAGS_SET(_p, NC_TOLERANT)
#define	NTCTX_READ_ONLY(_p)	(((_p)->nc_flags & NC_READ_ONLY) == \
				 NC_READ_ONLY)
#define	NTCTX_CF_OPEN(_p)	NTCTX_FLAGS_SET(_p, NC_CF_OPEN)
#define	NTCTX_VERIFIED(_p)	NTCTX_FLAGS_SET(_p, NC_OPEN|NC_VERIFIED)
#define	NTCTX_HEAD_VALID(_p)	NTCTX_FLAGS_SET(_p, NC_OPEN|NC_VERIFIED| \
					       NC_HEAD_VALID)
#define	NTCTX_READREADY(_p)	NTCTX_FLAGS_SET(_p, NC_OPEN|NC_VERIFIED| \
					       NC_HEAD_VALID|NC_VERSION_INIT)
#define	NTCTX_CFREADY(_p)	NTCTX_FLAGS_SET(_p, NC_OPEN|NC_VERIFIED| \
					       NC_HEAD_VALID|NC_VERSION_INIT| \
					       NC_HAVE_CFDEP|NC_CF_VERIFIED)
#define	NTCTX_WRITEABLE(_p)	(!NTCTX_READ_ONLY(_p) && NTCTX_READREADY(_p))
#define	NTCTX_WRITEREADY(_p)	(!NTCTX_READ_ONLY(_p) && NTCTX_CFREADY(_p))
#define	NTCTX_HAVE_PATH(_p)	(NTCTX_FLAGS_SET(_p, NC_HAVE_PATH) && \
				 (_p)->nc_path)
#define	NTCTX_HAVE_CF_PATH(_p)	(NTCTX_FLAGS_SET(_p, NC_HAVE_CF_PATH) && \
				 (_p)->nc_cf_path)
#define	NTCTX_HAVE_VERDEP(_p)	(NTCTX_FLAGS_SET(_p, NC_HAVE_VERDEP) &&	\
				 (_p)->nc_verdep)
#define	NTCTX_HAVE_CFDEP(_p)	(NTCTX_FLAGS_SET(_p, NC_HAVE_CFDEP) &&	\
				 (_p)->nc_cfdep)
#define	NTCTX_HAVE_IVBLOCK(_p)	(NTCTX_FLAGS_SET(_p, NC_HAVE_IVBLOCK) && \
				 (_p)->nc_ivblock)

/*
 * Version dispatch table - to handle different file format versions.
 */
typedef unsigned char vdt_dispatch_key_t[VERSION_SIZE];
#define	VDT_VERSION_KEY(_maj, _min)	{ (_min), (_maj), 64, 19 }
#define	VDT_MAJOR(_v)			((_v)->version[1])
#define	VDT_MINOR(_v)			((_v)->version[0])
typedef struct version_dispatch_table {
    vdt_dispatch_key_t	version;
    int		(*version_init)(nc_context_t *ntcp);
    int		(*version_verify)(nc_context_t *ntcp);
    int		(*version_finish)(nc_context_t *ntcp);
    int		(*version_seek)(nc_context_t *ntcp, u_int64_t block);
    int		(*version_readblock)(nc_context_t *ntcp, void *buffer);
    int		(*version_blockused)(nc_context_t *ntcp);
    int		(*version_writeblock)(nc_context_t *ntcp, void *buffer);
    int		(*version_sync)(nc_context_t *ntcp);
} v_dispatch_table_t;

/*
 * ntfsclone version 10 file format handling.
 */
#define	V10_DEFAULT_FACTOR	10		/* 1024 entries/index */

/*
 * Per-version specific handles.
 */
typedef struct version_10_context {
    unsigned char	*v10_bitmap;		/* Usage bitmap */
    u_int64_t		*v10_bucket_offset;	/* Precalculated indices */
    u_int64_t		v10_current_bucket;	/* Current bucket */
    u_int16_t		v10_bsfcount;		/* Preceding free blocks */
    u_int16_t		v10_bucket_factor;	/* log2(entries)/index */
} v10_context_t;

/*
 * Inline bitmap manipulation routines.
 */
static inline void
bitmap_set_bit(unsigned char *bitmap, u_int64_t bit, u_int32_t value)
{
    u_int64_t boffs = bit / 8;
    u_int8_t bdisp = bit & 7;
    unsigned char mask = 1 << bdisp;
    if (value) {
	bitmap[boffs] |= mask;
    } else {
	bitmap[boffs] &= ~mask;
    }
}

static inline u_int32_t
bitmap_bit_value(unsigned char *bitmap, u_int64_t bit)
{
    u_int64_t boffs = bit / 8;
    u_int8_t bdisp = bit & 7;
    unsigned char mask = 1 << bdisp;
    return ((bitmap[boffs] & mask) ? 1 : 0);
}

/*
 * v10_init	- Initialize version 10 file handling.
 *
 * - Allocate and initialize version 10 handle.
 * - Precalculate the CRC table.
 */
static int
v10_init(nc_context_t *ntcp)
{
    int error = EINVAL;
    v10_context_t *v10p;

    if (NTCTX_VALID(ntcp)) {
	if ((error = (*ntcp->nc_sysdep->sys_malloc)(&v10p, sizeof(*v10p))) == 0) {
	    int i;
	    memset(v10p, 0, sizeof(*v10p));
	    ntcp->nc_verdep = v10p;
	    ntcp->nc_flags |= (NC_HAVE_VERDEP|NC_VERSION_INIT);

	    if (ntcp->nc_cf_path && 
		((int) ntcp->nc_omode >= (int) SYSDEP_OPEN_RW)) {
		if ((error = cf_init(ntcp->nc_cf_path, ntcp->nc_sysdep,
				     ntcp->nc_head.cluster_size,
				     ntcp->nc_head.nr_clusters + 1, /* for trailing cluster */
				     &ntcp->nc_cf_handle)) == 0) {
		    ntcp->nc_flags |= NC_CF_OPEN;
		} else {
		    /*
		     * We'll create later...
		     */
		    error = 0;
		}
	    } else {
		if ((int) ntcp->nc_omode < (int) SYSDEP_OPEN_RW)
		    ntcp->nc_flags |= NC_READ_ONLY;
		else
		    /*
		     * Completely discard errors here.
		     */
		    error = 0;
	    }
	    v10p->v10_bucket_factor = V10_DEFAULT_FACTOR;
	}
    }
    return(error);
}

/*
 * v10_verify	- Verify the currently open file.
 *
 * - Load the bitmap
 * - Precalculate the count of preceding valid blocks.
 */
static int
v10_verify(nc_context_t *ntcp)
{
    int error = EINVAL;

    if (NTCTX_OPEN(ntcp)) {
	/*
	 * Verify the header magic.
	 */
	if (memcmp(ntcp->nc_head.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) == 0) {
	    v10_context_t *v10p = (v10_context_t *) ntcp->nc_verdep;
	    u_int64_t bmlen = ntcp->nc_head.nr_clusters / 8;

	    ntcp->nc_flags |= NC_HEAD_VALID;
	    /*
	     * Allocate and fill the bitmap.
	     */
	    if (ntcp->nc_head.nr_clusters & 7)
		bmlen++;
	    if (((error = (*ntcp->nc_sysdep->sys_malloc)(&v10p->v10_bitmap, 
							 bmlen)) == 0) &&
		((error = (*ntcp->nc_sysdep->sys_malloc)
		  (&v10p->v10_bucket_offset,
		   ((ntcp->nc_head.nr_clusters >> 
		     v10p->v10_bucket_factor)+1) * sizeof(u_int64_t))) == 0)) {
		u_int64_t cclust;
		int nconsecsync;

		memset(v10p->v10_bitmap, 0, bmlen);
		memset(v10p->v10_bucket_offset, 0,
		       ((ntcp->nc_head.nr_clusters >> 
			 v10p->v10_bucket_factor)+1) * sizeof(u_int64_t));

		/*
		 * Seek to the first offset.
		 */
		(void) (*ntcp->nc_sysdep->sys_seek)
		    (ntcp->nc_fd, ntcp->nc_head.offset_to_image_data,
		     SYSDEP_SEEK_ABSOLUTE, (u_int64_t *) NULL);

		/*
		 * We always handle the last cluster.
		 */
		ntcp->nc_head.nr_clusters++;

		/*
		 * Alas, there is no bitmap in the image, so we have to go
		 * and build it.
		 */
		cclust = 0;
		nconsecsync = 0;
		while (!error && (cclust < ntcp->nc_head.nr_clusters)) {
		    ntfsclone_atom_t ibuf;
		    u_int64_t rsize, iclust, cfoffs;
		    u_int64_t xcpos;
		    (void) (*ntcp->nc_sysdep->sys_seek)(ntcp->nc_fd, 0,
							SYSDEP_SEEK_RELATIVE,
							&xcpos);
		    if ((error = (*ntcp->nc_sysdep->sys_read)(ntcp->nc_fd,
							      &ibuf,
							      sizeof(ibuf),
							      &rsize))
			== 0) {
			switch (ibuf.nca_atype) {
			case 0:	/* empty cluster */
			    nconsecsync = 0;
			    cclust += ibuf.nca_union.ncau_empty_count;
			    break;
			case 1: /* used cluster */
			    nconsecsync = 0;
			    if ((error = (*ntcp->nc_sysdep->sys_seek)
				 (ntcp->nc_fd,
				  ntcp->nc_head.cluster_size -
				  sizeof(ibuf.nca_union),
				  SYSDEP_SEEK_RELATIVE,
				  &cfoffs)) == 0) {
				bitmap_set_bit(v10p->v10_bitmap, cclust, 1);
				if (v10p->v10_bucket_offset[cclust >> 
							    v10p->v10_bucket_factor]
				    == 0) {
				    /*
				     * First used cluster in bucket.  Make note
				     * of offset to the atom.
				     */
				    v10p->v10_bucket_offset[cclust >>
							    v10p->v10_bucket_factor]
					= cfoffs - ntcp->nc_head.cluster_size -
					ATOM_TO_DATA_OFFSET;
				}
				cclust++;
			    } else {
				if (NTCTX_TOLERANT(ntcp)) {
				    error = 0;
				    cclust = ntcp->nc_head.nr_clusters;
				}
			    }
			    break;
			default:
			    if (NTCTX_TOLERANT(ntcp)) {
				error = 0;
				if (nconsecsync > 128) {
				    cclust = ntcp->nc_head.nr_clusters;
				} else {	
				    nconsecsync++;
				}
			    } else {
				error = EDEADLK;
			    }
			    break;
			}
		    } else {
			if (NTCTX_TOLERANT(ntcp)) {
			    u_int64_t discpos;
			    (void) (*ntcp->nc_sysdep->sys_seek)
				(ntcp->nc_fd, sizeof(ibuf), 
				 SYSDEP_SEEK_RELATIVE, &discpos);
			    cclust++;
			    error = 0;
			}
		    }
		}
		if (!error && ntcp->nc_cf_handle) {
		    error = cf_verify(ntcp->nc_cf_handle);
		    if (!error) {
			ntcp->nc_flags |= NC_CF_VERIFIED;
		    }
		}
	    }
	}
    }
    return(error);
}

/*
 * v10_finish	- Finish version-specific handling.
 *
 * Free structures.
 */
static int
v10_finish(nc_context_t *ntcp)
{
    int error = EINVAL;

    if (NTCTX_HAVE_VERDEP(ntcp)) {
	v10_context_t *v10p = (v10_context_t *) ntcp->nc_verdep;

	if (v10p->v10_bitmap)
	    (void) (*ntcp->nc_sysdep->sys_free)(v10p->v10_bitmap);
	if (v10p->v10_bucket_offset)
	    (void) (*ntcp->nc_sysdep->sys_free)(v10p->v10_bucket_offset);
	(void) (*ntcp->nc_sysdep->sys_free)(v10p);
	ntcp->nc_flags &= ~NC_HAVE_VERDEP;
	error = (ntcp->nc_cf_handle) ? cf_finish(ntcp->nc_cf_handle) : 0;
    }
    return(error);
}

/*
 * v10_seek	- Version-specific handling for seeking to a particular block.
 *
 * Update the number of preceding valid blocks.
 */
static int
v10_seek(nc_context_t *ntcp, u_int64_t blockno)
{
    int error = EINVAL;

    if (NTCTX_HAVE_VERDEP(ntcp)) {
	v10_context_t *v10p = (v10_context_t *) ntcp->nc_verdep;
	int trailing_cluster_in_image = 
	    (VDT_MINOR(ntcp->nc_dispatch) >= 1) ? 1 : 0;
	u_int64_t pbn, cbn;

	/*
	 * The trailing cluster "should" be the same as the first cluster.
	 * If it's not in the image, then just magically redirect it.
	 */
	if (!trailing_cluster_in_image && 
	    (blockno == ntcp->nc_head.nr_clusters))
	    blockno = 0;

	cbn = blockno >> v10p->v10_bucket_factor;
	if (cbn != v10p->v10_current_bucket) {
	    /*
	     * Starting with the hint that is nearest, start calculating
	     * the preceding free blocks.
	     */
	    v10p->v10_bsfcount = 0;
	    pbn = cbn << v10p->v10_bucket_factor;
	    for (v10p->v10_bsfcount = 0;
		 ((bitmap_bit_value(v10p->v10_bitmap, pbn + v10p->v10_bsfcount) 
		   == 0) &&
		  ((pbn + v10p->v10_bsfcount) < blockno));
		 v10p->v10_bsfcount++)
		;
	    v10p->v10_current_bucket = cbn;
	}
	error = (ntcp->nc_cf_handle) ? cf_seek(ntcp->nc_cf_handle, blockno) : 0;
	    
    }
    return(error);
}

/*
 * seek2cluster	- Seek to the specified cluster in the image.
 */
static inline int
seek2cluster(nc_context_t *ntcp, u_int64_t cnum)
{
    int error = EINVAL;
    v10_context_t *v10p = (v10_context_t *) ntcp->nc_verdep;
    u_int64_t cbucket = cnum >> v10p->v10_bucket_factor;
    u_int64_t imgpos = v10p->v10_bucket_offset[cbucket];
    u_int64_t cpos, cfoffs;

    if (bitmap_bit_value(v10p->v10_bitmap, cnum) && imgpos) {
	if (cbucket == v10p->v10_current_bucket) {
	    cpos = (v10p->v10_current_bucket << v10p->v10_bucket_factor) + v10p->v10_bsfcount;
	} else {
	    /*
	     * Advance to the first valid block in the bucket.
	     */
	    for (cpos = cnum & ~((1 << v10p->v10_bucket_factor) - 1);
		 bitmap_bit_value(v10p->v10_bitmap, cpos) == 0;
		 cpos++)
		;
	}
	/*
	 * Now the tedium...
	 */
	error = (*ntcp->nc_sysdep->sys_seek)(ntcp->nc_fd,
					     imgpos,
					     SYSDEP_SEEK_ABSOLUTE,
					     (u_int64_t *) NULL);
	while (!error && (cpos < cnum)) {
	    ntfsclone_atom_t ibuf;
	    u_int64_t rsize;
	    if ((error = (*ntcp->nc_sysdep->sys_read)(ntcp->nc_fd,
						      &ibuf,
						      sizeof(ibuf),
						      &rsize))
		== 0) {
		switch (ibuf.nca_atype) {
		case 0:	/* empty cluster */
		    cpos += ibuf.nca_union.ncau_empty_count;
		    break;
		case 1: /* used cluster */
		    if ((error = (*ntcp->nc_sysdep->sys_seek)
			 (ntcp->nc_fd,
			  ntcp->nc_head.cluster_size -
			  sizeof(ibuf.nca_union),
			  SYSDEP_SEEK_RELATIVE,
			  &cfoffs)) == 0) {
			cpos++;
		    }
		    break;
		default:
		    error = EDEADLK;
		    break;
		}
	    }
	}
	/*
	 * Now, we are ostensibly at the right place.  Our count had
	 * better match...
	 */
	if (!error && (cpos == cnum)) {
	    error = (*ntcp->nc_sysdep->sys_seek)(ntcp->nc_fd,
						 ATOM_TO_DATA_OFFSET,
						 SYSDEP_SEEK_RELATIVE,
						 (u_int64_t *) NULL);
	}
    }
    return(error);
}

/*
 * v10_readblock	- Read the block at the current position.
 */
static int
v10_readblock(nc_context_t *ntcp, void *buffer)
{
    int error = EINVAL;

    /*
     * Check to see if we can get the result from the change file.
     */
    if (NTCTX_HAVE_VERDEP(ntcp)) {
	if (ntcp->nc_cf_handle) {
	    cf_seek(ntcp->nc_cf_handle, ntcp->nc_curblock);
	    error = cf_readblock(ntcp->nc_cf_handle, buffer);
	}
	if (error) {
	    v10_context_t *v10p = (v10_context_t *) ntcp->nc_verdep;

	    /*
	     * Determine whether the block is used/valid.
	     */
	    if (bitmap_bit_value(v10p->v10_bitmap, ntcp->nc_curblock)) {
		/* block is valid */
		if ((error = seek2cluster(ntcp, ntcp->nc_curblock)) == 0) {
		    u_int64_t r_size = -1;

		    (void) (*ntcp->nc_sysdep->sys_read)
			(ntcp->nc_fd, buffer, ntcp->nc_head.cluster_size,
			 &r_size);
		    /*
		     * XXX - endian?
		     */
		    if (r_size != ntcp->nc_head.cluster_size) {
			error = EIO;
		    }
		}
	    } else {
		/*
		 * If we're reading an invalid block, use the handy buffer.
		 */
		memcpy(buffer, ntcp->nc_ivblock, ntcp->nc_head.cluster_size);
		error = 0;	/* This shouldn't be necessary... */
	    }
	}
    }
    return(error);
}

/*
 * v10_blockused	- Is the current block in use?
 */
static int
v10_blockused(nc_context_t *ntcp)
{
    int retval = BLOCK_ERROR;
    if (NTCTX_HAVE_VERDEP(ntcp)) {
	v10_context_t *v10p = (v10_context_t *) ntcp->nc_verdep;

	retval = (ntcp->nc_cf_handle && cf_blockused(ntcp->nc_cf_handle)) ? 1 : 
	    bitmap_bit_value(v10p->v10_bitmap,ntcp->nc_curblock);
    }
    return(retval);
}

/*
 * v10_writeblock	- Write block at current location.
 */
static int
v10_writeblock(nc_context_t *ntcp, void *buffer)
{
    int error = EINVAL;

    /*
     * Make sure we're initialized.
     */
    if (NTCTX_HAVE_VERDEP(ntcp)) {
	if (!NTCTX_WRITEREADY(ntcp)) {
	    if (!NTCTX_HAVE_CF_PATH(ntcp)) {
		/*
		 * We have to make up a name.
		 */
		if ((error = 
		     (*ntcp->nc_sysdep->sys_malloc)(&ntcp->nc_cf_path,
						   strlen(ntcp->nc_path) +
						   strlen(cf_trailer) + 1)) 
		    == 0) {
		    memcpy(ntcp->nc_cf_path, ntcp->nc_path, strlen(ntcp->nc_path));
		    memcpy(&ntcp->nc_cf_path[strlen(ntcp->nc_path)],
			   cf_trailer, strlen(cf_trailer)+1);
		    ntcp->nc_flags |= NC_HAVE_CF_PATH;
		}
	    }
	    error = cf_create(ntcp->nc_cf_path, ntcp->nc_sysdep,
			      ntcp->nc_head.cluster_size, ntcp->nc_head.nr_clusters,
			      &ntcp->nc_cf_handle);
	    if (!error) {
		ntcp->nc_flags |= (NC_HAVE_CFDEP|NC_CF_VERIFIED);
	    }
	} else {
	    error = 0;
	}
	if (!error) {
	    cf_seek(ntcp->nc_cf_handle, ntcp->nc_curblock);
	    error = cf_writeblock(ntcp->nc_cf_handle, buffer);
	}
    }
    return(error);
}

/*
 * v10_sync	- Flush changes to change file
 */
static int
v10_sync(nc_context_t *ntcp)
{
    int error = EINVAL;
    if (NTCTX_WRITEREADY(ntcp)) {
	error = cf_sync(ntcp->nc_cf_handle);
    }
    return(error);
}

/*
 * Dispatch table for handling various versions.
 */
static const v_dispatch_table_t
version_table[] = {
    { VDT_VERSION_KEY(10, 1), 	/* version 10.1 */
      v10_init, v10_verify, v10_finish, v10_seek, v10_readblock, 
      v10_blockused, v10_writeblock, v10_sync },
    { VDT_VERSION_KEY(10, 0), 	/* version 10.0 */
      v10_init, v10_verify, v10_finish, v10_seek, v10_readblock, 
      v10_blockused, v10_writeblock, v10_sync },
};

/*
 * ntfsclone_close()	- Close the image handle.
 */
int
ntfsclone_close(void *rp)
{
    int error = EINVAL;
    nc_context_t *ntcp = (nc_context_t *) rp;

    if (NTCTX_VALID(ntcp)) {
	if (NTCTX_CF_OPEN(ntcp)) {
	    (void) (*ntcp->nc_dispatch->version_sync)(ntcp);
	}
	if (NTCTX_OPEN(ntcp)) {
	    (void) (*ntcp->nc_sysdep->sys_close)(ntcp->nc_fd);
	}
	if (NTCTX_HAVE_PATH(ntcp)) {
	    (void) (*ntcp->nc_sysdep->sys_free)(ntcp->nc_path);
	}
	if (NTCTX_HAVE_CF_PATH(ntcp)) {
	    (void) (*ntcp->nc_sysdep->sys_free)(ntcp->nc_cf_path);
	}
	if (NTCTX_HAVE_IVBLOCK(ntcp)) {
	    (void) (*ntcp->nc_sysdep->sys_free)(ntcp->nc_ivblock);
	}
	if (NTCTX_HAVE_VERDEP(ntcp)) {
	    if (ntcp->nc_dispatch && ntcp->nc_dispatch->version_finish)
		error = (*ntcp->nc_dispatch->version_finish)(ntcp);
	}
	(void) (*ntcp->nc_sysdep->sys_free)(ntcp);
	error = 0;
    }
    return(error);
}

/*
 * ntfsclone_open	- Open an image handle using the system-specific
 *			  interfaces.
 */
int
ntfsclone_open(const char *path, const char *cfpath, sysdep_open_mode_t omode,
	       const sysdep_dispatch_t *sysdep, void **rpp)
{
    int error = EINVAL;
    if (sysdep) {
	nc_context_t *ntcp;
	error = (*sysdep->sys_malloc)(&ntcp, sizeof(*ntcp));

	if (ntcp) {
	    memset(ntcp, 0, sizeof(*ntcp));
	    ntcp->nc_flags |= NC_VALID;
	    ntcp->nc_sysdep = sysdep;

	    if ((error = (*ntcp->nc_sysdep->sys_open)(&ntcp->nc_fd,
						     path,
						     SYSDEP_OPEN_RO)) == 0) {
		ntcp->nc_flags |= NC_OPEN;
		if ((error = 
		     (*ntcp->nc_sysdep->sys_malloc)(&ntcp->nc_path,
						   strlen(path)+1)) == 0) {
		    ntcp->nc_flags |= NC_HAVE_PATH;
		    ntcp->nc_omode = omode;
		    memcpy(ntcp->nc_path, path, strlen(path)+1);
		    if (cfpath &&
			((error = 
			  (*ntcp->nc_sysdep->sys_malloc)(&ntcp->nc_cf_path,
							strlen(cfpath)+1)) 
			 == 0)) {
			ntcp->nc_flags |= NC_HAVE_CF_PATH;
			memcpy(ntcp->nc_cf_path, cfpath, strlen(cfpath)+1);
		    }
		    if (!error)
			*rpp = (void *) ntcp;
		}
	    }
	    if (error) {
		ntfsclone_close(ntcp);
	    }
	}
    }
    if (error) {
	*rpp = (void *) NULL;
    }
    return(error);
}

/*
 * ntfsclone_tolerant_mode	- Set tolerant mode
 */
void
ntfsclone_tolerant_mode(void *rp)
{
    nc_context_t *ntcp = (nc_context_t *) rp;

    if (NTCTX_OPEN(ntcp)) {
	ntcp->nc_flags |= NC_TOLERANT;
    }
}

/*
 * ntfsclone_verify	- Determine the version of the file and verify it.
 */
static int
ntfsclone_verify_common(void *rp, int full)
{
    int error = EINVAL;
    nc_context_t *ntcp = (nc_context_t *) rp;

    if (NTCTX_OPEN(ntcp)) {
	u_int64_t r_size;

	/*
	 * Read the header.
	 */
	if (((error = 
	      (*ntcp->nc_sysdep->sys_read)(ntcp->nc_fd, &ntcp->nc_head,
					  sizeof(ntcp->nc_head), &r_size)) == 0) 
	    &&
	    (r_size == sizeof(ntcp->nc_head))) {
	    int veridx;
	    int found = -1;

	    /*
	     * Scan through the version table and find a match for the
	     * version string.
	     */
	    for (veridx = 0; 
		 veridx < sizeof(version_table)/sizeof(version_table[0]);
		 veridx++) {
		vdt_dispatch_key_t key = 
		    VDT_VERSION_KEY(ntcp->nc_head.major_ver,
				    ntcp->nc_head.minor_ver);
		if (memcmp(key, version_table[veridx].version, sizeof(key)) 
		    == 0) {
		    found = veridx;
		    break;
		}
	    }

	    /*
	     * See if we found a match.
	     */
	    if (found >= 0) {
		ntcp->nc_dispatch = (v_dispatch_table_t *) &version_table[found];
		/*
		 * Initialize the per-version handle.
		 */
		if (full && !(error = (*ntcp->nc_dispatch->version_init)(ntcp))) {
		    /*
		     * Verify the version header.
		     */
		    if (!(error = (*ntcp->nc_dispatch->version_verify)(ntcp))) {
			ntcp->nc_flags |= NC_VERIFIED;
			ntcp->nc_curblock = 0;
			/*
			 * Allocate a buffer.
			 */
			if ((error = 
			     (*ntcp->nc_sysdep->sys_malloc)
			     (&ntcp->nc_ivblock, ntcp->nc_head.cluster_size))
			    == 0) {
			    memset(ntcp->nc_ivblock, 69, 
				   ntcp->nc_head.cluster_size);
			    ntcp->nc_flags |= NC_HAVE_IVBLOCK;
			}
		    }
		}
	    } else {
		error = ENOENT;
	    }
	} else {
	    if (error == 0) 
		/*
		 * Implies:
		 * (r_size != sizeof(ntcp->nc_head)
		 */
		error = EIO;
	}
    }
    return(error);
}

static int
ntfsclone_verify_header_only(void *rp)
{
    return(ntfsclone_verify_common(rp, 0));
}

int
ntfsclone_verify(void *rp)
{
    return(ntfsclone_verify_common(rp, 1));
}

/*
 * ntfsclone_blocksize	- Return the blocksize.
 */
int64_t
ntfsclone_blocksize(void *rp)
{
    nc_context_t *ntcp = (nc_context_t *) rp;
    return((NTCTX_VERIFIED(ntcp)) ? ntcp->nc_head.cluster_size : -1);
}

/*
 * ntfsclone_blockcount	- Return the total count of blocks.
 */
int64_t
ntfsclone_blockcount(void *rp)
{
    nc_context_t *ntcp = (nc_context_t *) rp;
    return((NTCTX_VERIFIED(ntcp)) ? ntcp->nc_head.nr_clusters : -1);
}

/*
 * ntfsclone_seek	- Seek to a particular block.
 */
int
ntfsclone_seek(void *rp, u_int64_t blockno)
{
    int error = EINVAL;
    nc_context_t *ntcp = (nc_context_t *) rp;

    if (NTCTX_READREADY(ntcp) && (blockno <= ntcp->nc_head.nr_clusters)) {
	/*
	 * Use the version-specific seek routine to do the heavy lifting.
	 */
	if (!(error = (*ntcp->nc_dispatch->version_seek)(ntcp, blockno))) {
	    ntcp->nc_curblock = blockno;
	}
    }
    return(error);
}

/*
 * ntfsclone_tell	- Obtain the current position.
 */
u_int64_t
ntfsclone_tell(void *rp)
{
    nc_context_t *ntcp = (nc_context_t *) rp;

    return((NTCTX_READREADY(ntcp)) ? ntcp->nc_curblock : ~0);
}

/*
 * ntfsclone_readblocks	- Read blocks from the current position.
 */
int
ntfsclone_readblocks(void *rp, void *buffer, u_int64_t nblocks)
{
    int error = EINVAL;
    nc_context_t *ntcp = (nc_context_t *) rp;
    if (NTCTX_READREADY(ntcp)) {
	u_int64_t bindex;
	void *cbp = buffer;

	/*
	 * Iterate and use the version-specific routine to do the heavy
	 * lifting.
	 */
	for (bindex = 0; bindex < nblocks; bindex++) {
	    if ((error = (*ntcp->nc_dispatch->version_readblock)(ntcp, cbp))) {
		break;
	    }
	    ntcp->nc_curblock++;
	    cbp += ntcp->nc_head.cluster_size;
	}
    }
    return(error);
}

/*
 * ntfsclone_block_used	- Determine if the current block is used.
 */
int
ntfsclone_block_used(void *rp)
{
    nc_context_t *ntcp = (nc_context_t *) rp;
    return((NTCTX_READREADY(ntcp)) ? (*ntcp->nc_dispatch->version_blockused)(ntcp) :
	   BLOCK_ERROR);
}

/*
 * ntfsclone_writeblocks	- Write blocks to the current position.
 */
int
ntfsclone_writeblocks(void *rp, void *buffer, u_int64_t nblocks)
{
    int error = EINVAL;
    nc_context_t *ntcp = (nc_context_t *) rp;

    if (NTCTX_WRITEABLE(ntcp)) {
	u_int64_t bindex;
	void *cbp = buffer;

	/*
	 * Iterate and use the version-specific routine to do the heavy
	 * lifting.
	 */
	for (bindex = 0; bindex < nblocks; bindex++) {
	    if ((error = (*ntcp->nc_dispatch->version_writeblock)(ntcp, cbp))) {
		break;
	    }
	    ntcp->nc_curblock++;
	    cbp += ntcp->nc_head.cluster_size;
	}
    }
    return(error);
}

/*
 * ntfsclone_sync	- Commit changes to image.
 */
int
ntfsclone_sync(void *rp)
{
    nc_context_t *ntcp = (nc_context_t *) rp;

    return( (NTCTX_WRITEREADY(ntcp)) ?
	    (*ntcp->nc_dispatch->version_sync)(ntcp) :
	    EINVAL );
}

/*
 * ntfsclone_probe	- Is this a ntfsclone image?
 */
int
ntfsclone_probe(const char *path, const sysdep_dispatch_t *sysdep)
{
    void *testh = (void *) NULL;
    int error = ntfsclone_open(path, (char *) NULL, SYSDEP_OPEN_RO,
			       sysdep, &testh);
    if (!error) {
	error = ntfsclone_verify_header_only(testh);
	ntfsclone_close(testh);
    }
    return(error);
}

/*
 * The image type dispatch table.
 */
const image_dispatch_t ntfsclone_image_type = {
    "ntfsclone image",
    ntfsclone_probe,
    ntfsclone_open,
    ntfsclone_close,
    ntfsclone_tolerant_mode,
    ntfsclone_verify,
    ntfsclone_blocksize,
    ntfsclone_blockcount,
    ntfsclone_seek,
    ntfsclone_tell,
    ntfsclone_readblocks,
    ntfsclone_block_used,
    ntfsclone_writeblocks,
    ntfsclone_sync
};
