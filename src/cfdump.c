#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "sysdep_int.h"
#include "sysdep_posix.h"
#include "changefileint.h"

const sysdep_dispatch_t *sysdep = &posix_dispatch;

void
dump_header(char *n, cf_header_t *h) {
    printf("%s: version %d, flags 0x%04x: blocks %" PRIu64 " used/%" PRIu64 "total\n",
	   n, h->cf_version, h->cf_flags, h->cf_used_blocks,
	   h->cf_total_blocks);
}

static int crcinitdone = 0;
static u_int32_t xcrc_tab32[CRC_TABLE_LEN];
static inline u_int32_t
xcrc32(u_int32_t crc, unsigned char *buf, u_int64_t size)
{
    int i;
    u_int64_t s;
    u_int32_t tmp;

    if (!crcinitdone) {
	for (i=0; i<CRC_TABLE_LEN; i++) {
	    int j;
	    u_int32_t init_crc = (u_int32_t) i;
	    for (j=0; j<CRC_UNIT_BITS; j++) {
		init_crc = (init_crc & 0x00000001L) ?
		    (init_crc >> 1) ^ 0xEDB88320L :
		    (init_crc >> 1);
	    }
	    xcrc_tab32[i] = init_crc;
	}
    }
    for (s=0; s<size; s++) {
	tmp = crc ^ (((u_int32_t) buf[s]) & 0x000000ffL);
	crc = (crc >> 8) ^ xcrc_tab32[ tmp & 0xff ];
    }
    return(crc);
}

int
verify_block(void *cf, u_int64_t offs, u_int64_t index, void *rbuffer, 
	     u_int64_t bsize)
{
    u_int64_t nread;
    int error = 1;
    if (!(error = (*sysdep->sys_seek)(cf, offs, SYSDEP_SEEK_ABSOLUTE,
				      (u_int64_t *) NULL))) {
	if (!(error = (*sysdep->sys_read)(cf, rbuffer, bsize, &nread)) &&
	    (nread == bsize)) {
	    cf_block_trailer_t btrail;
	    if (!(error = (*sysdep->sys_read)
		  (cf, &btrail, sizeof(btrail), &nread)) &&
		(nread == sizeof(btrail))) {
		if ((btrail.cfb_curblock == index) &&
		    (btrail.cfb_magic == CF_MAGIC_3) &&
		    (btrail.cfb_crc == xcrc32(0L,
					      rbuffer,
					      bsize))) {
		    error = 0;
		} else {
		    error = 1;
		}
	    }
	}
    }
    return(error);
}

void
dump_blocks(cf_header_t *h, u_int64_t *bm, void *cf) {
    u_int64_t bi, nread;
    u_int64_t nfound = 0;
    u_int64_t bsize = 0;
    void *rbuffer = (void *) NULL;
    int error;
    for (bi=0; bi<h->cf_total_blocks; bi++) {
	if (bm[bi]) {
	    int good = 0;
	    printf("%lu: offset 0x%016lx: ", bi, bm[bi]);
	    nfound++;
	    if (bsize == 0) {
		for (bsize = 512; bsize < (128*1024*1024); bsize *= 2) {
		    if (rbuffer) {
			(void) (*sysdep->sys_free)(rbuffer);
		    }
		    (void) (*sysdep->sys_malloc)(&rbuffer, bsize);
		    if (rbuffer) {
			if (!verify_block(cf, bm[bi], bi, rbuffer, bsize)) {
			    good = 1;
			    break;
			}
		    }
		}
	    } else {
		if (!verify_block(cf, bm[bi], bi, rbuffer, bsize)) {
		    good = 1;
		}
	    }
	    printf("%s\n", (good) ? "ok" : "INVALID");
	    if (bsize && rbuffer) {
		u_int64_t boff;
		for (boff = 0; boff < bsize; boff += 16) {
		    u_int64_t soff;
		    printf("0x%04x: ", (u_int16_t) boff);
		    for (soff = 0; soff < 16; soff++) {
			printf("%02x ", ((unsigned char *) rbuffer)[boff+soff]);
		    }
		    for (soff = 0; soff < 16; soff++) {
			printf("%c", 
			       isalnum(((unsigned char *) rbuffer)[boff+soff])?
			       ((unsigned char *) rbuffer)[boff+soff] :
			       ((((unsigned char *) rbuffer)[boff+soff]) ? '.' :
				' '));
		    }
		    printf("\n");
		}
	    }
	}
    }
    if (h->cf_used_blocks != nfound) {
	printf("WARNING: %lu found, %" PRIu64 " used blocks\n", nfound, 
	       h->cf_used_blocks);
    }
}

int
main(int argc, char *argv[])
{
    void *cfp;
    int i, error;

    for (i=1; i<argc; i++) {
	if ((error = (*sysdep->sys_open)(&cfp, argv[i], SYSDEP_OPEN_RO)) == 0) {
	    cf_header_t header;
	    u_int64_t nread;
	    (void) (*sysdep->sys_seek)(cfp, 0, SYSDEP_SEEK_ABSOLUTE,
				       (u_int64_t *) NULL);
	    if ((error = (*sysdep->sys_read)(cfp, &header, sizeof(header),
					     &nread)) == 0) {
		if ((header.cf_magic == CF_MAGIC_1) &&
		    (header.cf_magic2 == CF_MAGIC_2)) {
		    u_int64_t bmsize = header.cf_total_blocks *
			sizeof(u_int64_t);
		    u_int64_t *blockmap;
		    
		    if ((error = (*sysdep->sys_malloc)(&blockmap, bmsize))
			== 0) {
			dump_header(argv[i], &header);
			(void) (*sysdep->sys_seek)(cfp, 
						   header.cf_blockmap_offset,
						   SYSDEP_SEEK_ABSOLUTE,
						   (u_int64_t *) NULL);
			if ((error = (*sysdep->sys_read)(cfp, blockmap,
							 bmsize, &nread))
			    == 0) {
			    dump_blocks(&header, blockmap, cfp);
			} else {
			    fprintf(stderr, "%s: cannot read blockmap\n",
				    argv[i]);
			}
		    } else {
			fprintf(stderr, "%s: cannot allocate blockmap\n", 
				argv[i]);
		    }
		} else {
		    fprintf(stderr, "%s: invalid header\n", argv[i]);
		}
	    } else {
		fprintf(stderr, "%s: cannot read header\n", argv[i]);
	    }
	    (void) (*sysdep->sys_close)(cfp);
	} else {
	    fprintf(stderr, "%s: cannot open %s\n", argv[0], argv[i]);
	    break;
	}
    }
    return(error);
}
