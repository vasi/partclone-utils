/*
 * changefile.c	- Changefile handling.
 */
/*
 * @(#) $RCSfile: $ $Revision: $ (Ideal World, Inc.) $Date: $
 */
/*
 * HISTORY
 * $Log: $
 * $EndLog$
 */
/*
 * Copyright (c) 2011, Ideal World, Inc.  All Rights Reserved.
 */
#ifdef	HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <errno.h>
#include <string.h>
#include "changefile.h"
#include "changefileint.h"

/*
 * cf_init	- Initialize change file handling.
 *
 * Allocate and initialize change file handle.
 */
int
cf_init(const char *cfpath, const sysdep_dispatch_t *sysdep, 
	u_int64_t blocksize, u_int64_t blockcount, void **cfpp)
{
    int error = EINVAL;
    cf_context_t *cfp = (cf_context_t *) NULL;

    if ((error = (*sysdep->sys_malloc)(&cfp, sizeof(*cfp))) == 0) {
	int i;
	memset(cfp, 0, sizeof(*cfp));

	/*
	 * Open the file.
	 */
	if ((error = (*sysdep->sys_open)(&cfp->cfc_fd, cfpath, SYSDEP_OPEN_RW)) 
	    == 0) {
	    cfp->cfc_sysdep = sysdep;
	    cfp->cfc_blocksize = blocksize;
	    cfp->cfc_blockcount = blockcount;
	    /*
	     * Initialize the CRC table.
	     */
	    for (i=0; i<CRC_TABLE_LEN; i++) {
		int j;
		u_int32_t init_crc = (u_int32_t) i;
		for (j=0; j<CRC_UNIT_BITS; j++) {
		    init_crc = (init_crc & 0x00000001L) ?
			(init_crc >> 1) ^ 0xEDB88320L :
			(init_crc >> 1);
		}
		cfp->cfc_crc_tab32[i] = init_crc;
	    }
	} else {
	    (void) (*sysdep->sys_free)(cfp);
	    cfp = (cf_context_t *) NULL;
	}
    }
    if (!error && cfp)
	*cfpp = (void *) cfp;
    return(error);
}

/*
 * cf_verify	- Verify the change file.
 *
 * - Load the blockmap.
 */
int
cf_verify(void *vcp)
{
    int error = EINVAL;
    cf_context_t *cfp = (cf_context_t *) vcp;
    u_int64_t nread;

    /*
     * Make sure we're at the beginning of the file.
     */
    (void) (*cfp->cfc_sysdep->sys_seek)(cfp->cfc_fd,
					0,
					SYSDEP_SEEK_ABSOLUTE,
					(u_int64_t *) NULL);
    if (((error = (*cfp->cfc_sysdep->sys_read)(cfp->cfc_fd,
					       &cfp->cfc_header,
					       sizeof(cfp->cfc_header),
					       &nread)) == 0) &&
	(nread == sizeof(cfp->cfc_header))) {
	/*
	 * Verify read header.
	 */
	if ((cfp->cfc_header.cf_magic == CF_MAGIC_1) &&
	    (cfp->cfc_header.cf_magic2 == CF_MAGIC_2) &&
	    /* [2013-12] ntfs chicanery could have added the trailing block */
	    ((cfp->cfc_header.cf_total_blocks == cfp->cfc_blockcount) ||
	     (cfp->cfc_header.cf_total_blocks == (cfp->cfc_blockcount+1)))) {
	    u_int64_t bhsize = cfp->cfc_header.cf_total_blocks *
		sizeof(*cfp->cfc_blockmap);
	    /*
	     * Allocate, find and read the blockmap.
	     */
	    if ((error = 
		 (*cfp->cfc_sysdep->sys_malloc)(&cfp->cfc_blockmap, bhsize))
		== 0) {
		if ((error = (*cfp->cfc_sysdep->sys_seek)(cfp->cfc_fd,
							  cfp->cfc_header.
							  cf_blockmap_offset,
							  SYSDEP_SEEK_ABSOLUTE,
							  (u_int64_t *) NULL)
			) == 0) {
		    if (((error =
			  (*cfp->cfc_sysdep->sys_read)(cfp->cfc_fd,
						       cfp->cfc_blockmap,
						       bhsize,
						       &nread)) == 0) &&
			(nread == bhsize)) {
			/* xxx */
		    } else {
			if (error == 0)
			    error = EIO;
		    }
		}
	    }
	} else {
	    error = ENODEV;
	}
    } else {
	if (error == 0) {
	    /* Implies rsize != sizeof(cfp->cfc_header) */
	    error = EIO;
	}
    }
    return(error);
}

/*
 * cf_create	- Create change file if necessary.
 */
int
cf_create(const char *cfpath, const sysdep_dispatch_t *sysdep,
	  u_int64_t blocksize, u_int64_t blockcount, void **cfpp)
{
    int error;
    void *cfh = (void *) NULL;

    /*
     * First try to open an existing change file.
     */
    if ((error = (*sysdep->sys_open)(&cfh, cfpath, SYSDEP_OPEN_RW)) != 0) {
	/*
	 * Open failed - try to create it.
	 */
	if ((error = (*sysdep->sys_open)(&cfh,cfpath, SYSDEP_CREATE)) == 0) {
	    cf_header_t ncfh;
	    u_int64_t *bmp;
	    /*
	     * A new file!
	     */
	    ncfh.cf_magic = CF_MAGIC_1;
	    ncfh.cf_version = CF_VERSION_1;
	    ncfh.cf_flags = 0;
	    ncfh.cf_total_blocks = blockcount;
	    ncfh.cf_used_blocks = 0;
	    ncfh.cf_blockmap_offset = sizeof(ncfh);
	    ncfh.cf_magic2 = CF_MAGIC_2;
	    if ((error = (*sysdep->sys_malloc)(&bmp, 
					       blockcount * 
					       sizeof(u_int64_t))) 
		== 0) {
		u_int64_t nwritten;
		memset(bmp, 0, blockcount * sizeof(u_int64_t));
		if (((error = (*sysdep->sys_write)(cfh,
						   &ncfh,
						   sizeof(ncfh),
						   &nwritten)) == 0) &&
		    (nwritten == sizeof(ncfh)) &&
		    ((error = (*sysdep->sys_write)(cfh,
						   bmp,
						   blockcount *
						   sizeof(u_int64_t),
						   &nwritten))
		     == 0) &&
		    (nwritten == (blockcount * sizeof(u_int64_t)))) {
		    /* close it - we'll open it again below. */
		    (void) (*sysdep->sys_close)(cfh);
		}
		(void) (*sysdep->sys_free)(bmp);
	    }
	}
    }
    if (!error) {
	/*
	 * If we are successful thus far, then we have a
	 * candidate change file.
	 */
	if ((error = cf_init(cfpath, sysdep, 
			     blocksize, blockcount, cfpp)) == 0) {
	    error = cf_verify(*cfpp);
	}
    }
    return(error);
}

/*
 * cf_sync	- Sync change file changes to image.
 */
int
cf_sync(void *vcp)
{
    int error = EROFS;
    cf_context_t *cfp = (cf_context_t *) vcp;
    cf_header_t oheader = cfp->cfc_header;
    u_int64_t nwritten;

    oheader.cf_flags &= ~CF_HEADER_DIRTY;
    /*
     * Seek and write the sanitized header and block map.
     */
    if (((error = (*cfp->cfc_sysdep->sys_seek)(cfp->cfc_fd,
					       0,
					       SYSDEP_SEEK_ABSOLUTE,
					       (u_int64_t *) NULL)) == 0) &&
	((error = (*cfp->cfc_sysdep->sys_write)(cfp->cfc_fd,
						&oheader,
						sizeof(oheader),
						&nwritten)) == 0) &&
	(nwritten == sizeof(oheader)) &&
	((error = (*cfp->cfc_sysdep->sys_seek)(cfp->cfc_fd,
					       oheader.
					       cf_blockmap_offset,
					       SYSDEP_SEEK_ABSOLUTE,
					       (u_int64_t *) NULL))  == 0) &&
	((error = (*cfp->cfc_sysdep->sys_write)(cfp->cfc_fd,
						cfp->cfc_blockmap,
						oheader.cf_total_blocks *
						sizeof(u_int64_t),
						&nwritten)) == 0) &&
	(nwritten == (oheader.cf_total_blocks * sizeof(u_int64_t)))) {
	/*
	 * If successful, then we're no longer dirty.
	 */
	cfp->cfc_header.cf_flags &= ~CF_HEADER_DIRTY;
    }
    return(error);
}

/*
 * cf_finish	- Finish change file handling.
 *
 * Free structures.
 */
int
cf_finish(void *vcp)
{
    cf_context_t *cfp = (cf_context_t *) vcp;

    /*
     * See if we're dirty first and flush what we have if we are.
     */
    if (cfp->cfc_header.cf_flags & CF_HEADER_DIRTY)
	(void) cf_sync(vcp);
    if (cfp->cfc_blockmap)
	(void) (*cfp->cfc_sysdep->sys_free)(cfp->cfc_blockmap);
    (void) (*cfp->cfc_sysdep->sys_close)(cfp->cfc_fd);
    return((*cfp->cfc_sysdep->sys_free)(cfp));
}

/*
 * cf_seek	- Change file handling for seeking to a particular block.
 */
int
cf_seek(void *vcp, u_int64_t blockno)
{
    int error = ENXIO;
    cf_context_t *cfp = (cf_context_t *) vcp;
    if (blockno <= cfp->cfc_header.cf_total_blocks) {
	cfp->cfc_curpos = blockno;
	error = 0;
    }
    return(error);
}

/*
 * CRC routine.
 */
static inline u_int32_t
cf_crc32(cf_context_t *cfp, u_int32_t crc, unsigned char *buf, u_int64_t size)
{
    u_int64_t s;
    u_int32_t tmp;

    for (s=0; s<size; s++) {
	tmp = crc ^ (((u_int32_t) buf[s]) & 0x000000ffL);
	crc = (crc >> 8) ^ cfp->cfc_crc_tab32[ tmp & 0xff ];
    }
    return(crc);
}

/*
 * cf_readblock	- Read the block at the current position.
 */
int
cf_readblock(void *vcp, void *buffer)
{
    int error = EINVAL;
    cf_context_t *cfp = (cf_context_t *) vcp;

    /*
     * Check the block map for an offset.
     */
    if (cfp->cfc_blockmap[cfp->cfc_curpos]) {
	/*
	 * If present, seek and read the block and trailer.
	 */
	if ((error = (*cfp->cfc_sysdep->sys_seek)(cfp->cfc_fd,
						  cfp->cfc_blockmap
						  [cfp->cfc_curpos],
						  SYSDEP_SEEK_ABSOLUTE,
						  (u_int64_t *) NULL)) == 0) {
	    u_int64_t rsize = cfp->cfc_blocksize;
	    cf_block_trailer_t btrail;
	    u_int64_t nread;
	    if (((error = (*cfp->cfc_sysdep->sys_read)(cfp->cfc_fd,
						       buffer,
						       rsize,
						       &nread)) == 0) &&
		(nread == rsize)) {
		rsize = sizeof(cf_block_trailer_t);
		if (((error = (*cfp->cfc_sysdep->sys_read)(cfp->cfc_fd,
							   &btrail,
							   rsize,
							   &nread)) == 0) &&
		    (nread == rsize)) {
		    /*
		     * Verify the trailer.
		     */
		    if ((btrail.cfb_curblock == cfp->cfc_curpos) &&
			(btrail.cfb_magic == CF_MAGIC_3) &&
			(btrail.cfb_crc == cf_crc32(cfp,
						    0L,
						    buffer,
						    cfp->cfc_blocksize))) {
			error = 0;
		    } else {
			error = ESRCH;
		    }
		} else {
		    if (!error)
			error = EIO;
		}
	    } else {
		if (!error)
		    error = EIO;
	    }
	}
    } else {
	error = ENXIO;
    }
    return(error);
}

/*
 * cf_blockused	- Is the current block in use?
 */
int
cf_blockused(void *vcp)
{
    cf_context_t *cfp = (cf_context_t *) vcp;
    
    return((cfp->cfc_blockmap[cfp->cfc_curpos]) ? 1 : 0);
}

/*
 * cf_writeblock	- Write block at current location.
 */
int
cf_writeblock(void *vcp, void *buffer)
{
    int error = EROFS;
    cf_context_t *cfp = (cf_context_t *) vcp;
    u_int64_t nbloffs = cfp->cfc_blockmap[cfp->cfc_curpos];
    u_int64_t curpos;

    if ((error = (*cfp->cfc_sysdep->sys_seek)(cfp->cfc_fd,
					      nbloffs,
					      (nbloffs) ?
					      SYSDEP_SEEK_ABSOLUTE :
					      SYSDEP_SEEK_END,
					      &curpos)) == 0) {
	cf_block_trailer_t btrail = { cfp->cfc_curpos, 
				      cf_crc32(cfp, 0, buffer,
					       cfp->cfc_blocksize), 
				      CF_MAGIC_3 };
	u_int64_t nwritten;
	if (((error = (*cfp->cfc_sysdep->sys_write)(cfp->cfc_fd,
						    buffer,
						    cfp->cfc_blocksize,
						    &nwritten)) == 0) &&
	    (nwritten == cfp->cfc_blocksize) &&
	    ((error = (*cfp->cfc_sysdep->sys_write)(cfp->cfc_fd,
						    &btrail,
						    sizeof(btrail),
						    &nwritten)) == 0) &&
	    (nwritten == sizeof(btrail))) {
	    /*
	     * Write success.
	     */
	    if (!nbloffs) {
		/*
		 * We made a new block.
		 */
		cfp->cfc_blockmap[cfp->cfc_curpos] = curpos;
		cfp->cfc_header.cf_used_blocks++;
		cfp->cfc_header.cf_flags |= CF_HEADER_DIRTY;
	    }
	}
    }
    return(error);
}
