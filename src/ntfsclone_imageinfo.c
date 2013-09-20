/*
 * ntfsclone_imageinfo	- A cursory check that a file looks OK.
 */
/*
 * Copyright (c) 2013, Ideal World, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */
#ifdef	HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "libntfsclone.h"
#include "sysdep_posix.h"
#include "ntfsclone.h"

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

typedef struct version_10_context {
    unsigned char	*v10_bitmap;		/* Usage bitmap */
    off_t		*v10_bucket_offset;	/* Precalculated indices */
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

int
main(int argc, char *argv[])
{
  int i;

  for (i=1; i<argc; i++) {
    int error;
    void *ntctx;
    int dontcare = 0;
    int anomalies = 0;

    if ((error = ntfsclone_open(argv[i], (char *) NULL, SYSDEP_OPEN_RO,
				&posix_dispatch, &ntctx)) == 0) {
      u_int64_t bmscanned = 0, unset = 0, set = 0, strange = 0;
      u_int64_t lastset = 0, laststrange = 0;
      if (((error = ntfsclone_verify(ntctx)) == 0) || dontcare) {
	nc_context_t *p = (nc_context_t *) ntctx;
	v10_context_t *v = (v10_context_t *) p->nc_verdep;
	u_int64_t bmi;
	unsigned char *iob;

	if (dontcare && error)
	  p->nc_flags |= 4;
	for (bmi = 0; bmi < p->nc_head.nr_clusters; bmi++) {
	  switch (bitmap_bit_value(v->v10_bitmap,bmi)) {
	  case 0:
	    unset++; break;
	  case 1:
	    set++; lastset = bmi; break;
	  default:
	    anomalies++;
	    break;
	  }
	  bmscanned++;
	}
	fprintf(stdout, "%s: %" PRId64 " blocks, %" PRIu64 " blocks scanned, %" PRIu64 " unset, %" PRIu64 " set, %" PRIu64 " strange\n",
		argv[i], p->nc_head.nr_clusters, bmscanned, unset, set, strange);
	if ((iob = (unsigned char *) malloc(ntfsclone_blocksize(ntctx)))) {
	  int *fd = (int *) p->nc_fd;
	  off_t sblkpos;
	  off_t fsize;
	  struct stat sbuf;
	  error = ntfsclone_seek(ntctx, 0);
	  error = ntfsclone_readblocks(ntctx, iob, 1);
	  sblkpos = p->nc_head.offset_to_image_data;
	  fstat(*fd, &sbuf);
	  fsize = sbuf.st_size;
	  fprintf(stdout, "%s: size is %lld bytes, blocks (%lld bytes) start at %lld\n",
		  argv[i], (long long) fsize, (long long) ntfsclone_blocksize(ntctx), (long long) sblkpos);
	  /*
	   * Determine last block.
	   */
	  for (lastset = p->nc_head.nr_clusters-1;
	       lastset > 0 && (bitmap_bit_value(v->v10_bitmap, lastset) == 0);
	       lastset--)
	    ;
	  if ((error = ntfsclone_seek(ntctx, lastset)) == 0) {
	    if ((error = ntfsclone_readblocks(ntctx, iob, 1)) == 0) {
	      off_t cpos, eofpos;

	      cpos = lseek(*fd, 0, SEEK_CUR);
	      eofpos = lseek(*fd, 0, SEEK_END);
	      if (cpos == eofpos) {
		fprintf(stdout, "%s: read last block at end of file\n",
			argv[i]);
	      } else {
		fprintf(stderr, "%s: position after last block = %ld, eof position = %ld, blocksize = %" PRIu64 "\n",
			argv[i], cpos, eofpos, ntfsclone_blocksize(ntctx));
		anomalies++;
	      }
	    } else {
	      fprintf(stderr, "%s: cannot read block %" PRIu64 ", error = %d\n",
		      argv[i], lastset, error);
	      anomalies++;
	    }
	  } else {
	    fprintf(stderr, "%s: cannot seek to block %" PRIu64 ", error = %d\n",
		    argv[i], lastset, error);
	    anomalies++;
	  }
	  free(iob);
	} else {
	  fprintf(stderr, "%s: cannot malloc %" PRId64 " bytes\n", argv[i],
		  ntfsclone_blocksize(ntctx));
	  anomalies++;
	}
      } else {
	fprintf(stderr, "%s: cannot verify image (error = %d)\n", argv[i], error);
	anomalies++;
      }
    } else {
      fprintf(stderr, "%s: cannot open image (error = %d)\n", argv[i], error);
      anomalies++;
    }
    if (anomalies) {
      fprintf(stdout, "!!! %s: %d problems\n", argv[i], anomalies);
    } else {
      fprintf(stdout, "%s: OK\n", argv[i]);
    }
  }
}
