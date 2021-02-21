/*
 * libpartclone.h	- Interfaces to partclone library.
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

#ifndef _LIBPARTCLONE_H_
#define _LIBPARTCLONE_H_ 1
#include "partclone.h"
#include "sysdep_int.h"
#include <sys/types.h>

#define MAGIC_LEN 8

int      partclone_open(const char *path, const char *cfpath,
                        sysdep_open_mode_t omode, const sysdep_dispatch_t *sysdep,
                        void **rpp);
int      partclone_close(void *rp);
int      partclone_verify(void *rp);
int64_t  partclone_blocksize(void *rp);
int64_t  partclone_blockcount(void *rp);
int      partclone_seek(void *rp, uint64_t blockno);
uint64_t partclone_tell(void *rp);
int      partclone_readblocks(void *rp, void *buffer, uint64_t nblocks);
int      partclone_block_used(void *rp);
int      partclone_writeblocks(void *rp, void *buffer, uint64_t nblocks);
int      partclone_sync(void *rp);

typedef struct libpc_context {
    void *                         pc_fd;        /* File handle */
    char *                         pc_path;      /* Path to image */
    char *                         pc_cf_path;   /* Path to change file */
    void *                         pc_cf_handle; /* Change file handle */
    unsigned char *                pc_ivblock;   /* Convenient invalid block */
    void *                         pc_verdep;    /* Version-dependent handle */
    struct version_dispatch_table *pc_dispatch; /* Version-dependent dispatch */
    const sysdep_dispatch_t *      pc_sysdep;   /* System-specific routines */
    union {
        image_head_v1 pc_head_v1;
        image_head_v2 pc_head_v2;
    };
    struct {
        unsigned long long head_size;
        unsigned int       block_size;
        unsigned long long totalblock;
        unsigned short     checksum_size;
        unsigned long long device_size;
        unsigned int       blocks_per_checksum;
    } pc_head;
    uint64_t           pc_curblock; /* Current position */
    uint32_t           pc_flags;    /* Handle flags */
    sysdep_open_mode_t pc_omode;    /* Open mode */
} pc_context_t;

#endif /* _LIBPARTCLONE_H_ */
