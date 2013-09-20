/*
 * libpctest.c	- Test libpartclone
 */
/*
 * @(#) $RCSfile: libpctest.c,v $ $Revision: 1.2 $ (Ideal World, Inc.) $Date: 2010/07/17 20:47:32 $
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
#include "libpartclone.h"
#include "sysdep_posix.h"

int
main(int argc, char *argv[])
{
  int i;

  for (i=1; i<argc; i++) {
    int error;
    void *pctx;

    if ((error = partclone_open(argv[i], (char *) NULL, SYSDEP_OPEN_RW,
				&posix_dispatch, &pctx)) == 0) {
      if ((error = partclone_verify(pctx)) == 0) {
	int ci, ofd;
	char outfile[1024];
	size_t bsize = partclone_blocksize(pctx);
	size_t btotal = partclone_blockcount(pctx);
	u_int64_t freeblock = 0;
	unsigned char *iobuf = (unsigned char *) malloc(bsize);
	printf("%s: open success, blocksize is %zu, nblocks is %zu\n", argv[i], 
	       bsize, btotal);
	sprintf(outfile, "%s.out", argv[i]);
	if ((ofd = open(outfile, O_WRONLY|O_CREAT|O_LARGEFILE, 0600)) >= 0) {
	  size_t nwritten = 0, ntotal = 0, nskipped = 0;
	  for (ci=0; ci<btotal; ci++) {
	    if ((error = partclone_seek(pctx, ci)) == 0) {
	      ntotal++;
	      int blkstat = partclone_block_used(pctx);
	      if (blkstat == 1) {
		if (lseek(ofd, ci*bsize, SEEK_SET) != -1) {
		  if ((error = partclone_readblocks(pctx, iobuf, 1)) == 0) {
		    ssize_t nbout = write(ofd, iobuf, bsize);
		    if (nbout != bsize) {
		      error = (nbout < 0) ? errno : EIO;
		      printf("%s: write error %d at block %d\n",
			     outfile, error, ci);
		      break;
		    } else {
		      nwritten++;
		    }
		  } else {
		    printf("%s: read error %d at block %d\n",
			   argv[i], error, ci);
		    break;
		  }
		} else {
		  error = errno;
		  printf("%s: seek error %d at block %d\n",
			 outfile, error, ci);
		  break;
		}
	      } else if (blkstat != 0) {
		error = EIO;
		printf("%s: block %d status %d\n", argv[i], ci, blkstat);
		break;
	      } else {
		if ((blkstat == 0) && (freeblock == 0))
		    freeblock = ci;
		nskipped++;
	      }
	    } else {
	      printf("%s: seek error %d at block %d\n", argv[i], error, ci);
	      break;
	    }
	  }
	  close(ofd);
	  if (!error) {
	    printf("%s: complete: ", outfile);
	  } else {
	    printf("%s: bad: ", outfile);
	  }
	  printf("%zu blocks, %zu done, %zu written, %zu skipped\n",
		 btotal, ntotal, nwritten, nskipped);
	  /*
	   * test writing...
	   */
	  if (freeblock) {
	      char *iostring = "hello kitty";
	      u_int64_t iooffset = 23;
	      char cfname[1024];
	      memset(iobuf, 0, bsize);
	      memcpy(&iobuf[iooffset], iostring, strlen(iostring));
	      if ((error = partclone_seek(pctx, freeblock)) == 0) {
		  if ((error = partclone_writeblocks(pctx, iobuf, 1)) == 0) {
		      printf("write test success! wrote \"%s\" to block %" PRIu64 " offset %" PRIu64 "\n",
			     iostring, freeblock, iooffset);
		      /*
		       * Now read it back.
		       */
		      partclone_close(pctx);
		      sprintf(cfname, "%s.cf", argv[i]);
		      if (((error = partclone_open(argv[i], 
						   cfname, 
						   SYSDEP_OPEN_RW,
						   &posix_dispatch,
						   &pctx)) == 0) &&
			  ((error = partclone_verify(pctx)) == 0) &&
			  ((error = partclone_seek(pctx, freeblock)) == 0) &&
			  ((error = partclone_readblocks(pctx, iobuf, 1)) 
			   == 0)) {
			  if (memcmp(&iobuf[iooffset], iostring,
				     strlen(iostring)) == 0) {
			      printf("read back test success!\n");
			  } else {
			      printf("read back verify failed\n");
			  }
		      } else {
			  printf("read back failed with %d\n", error);
		      }
		  } else {
		      printf("image write error %d\n", error);
		  }
	      } else {
		  printf("image write seek error %d\n", error);
	      }
	  }
	} else {
	  error = errno;
	  printf("%s: open error %d\n", outfile, error);
	}
      } else {
	printf("%s: verify error %d\n", argv[i], error);
      }
      partclone_close(pctx);
    } else {
      printf("%s: open error %d\n", argv[i], error);
    }
  }
}
