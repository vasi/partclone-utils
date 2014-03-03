/*
 * changefileint.h	- Internals to the changefile library.
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
#ifndef	_CHANGEFILEINT_H_
#define	_CHANGEFILEINT_H_	1

#define	CRC_UNIT_BITS	8
#define	CRC_TABLE_LEN	(1<<CRC_UNIT_BITS)

/*
 * Change file header.
 */
#define	CF_MAGIC_1	0xdeadbeef
#define	CF_MAGIC_2	0xfeedf00d
#define	CF_MAGIC_3	0x3a070045
#define	CF_VERSION_1	1
#define	CF_HEADER_DIRTY	1
typedef struct change_file_header {
    u_int32_t	cf_magic;		/* 0x00 - magic */
    u_int16_t	cf_version;		/* 0x04 - version */
    u_int16_t	cf_flags;		/* 0x06 - flags */
    u_int64_t	cf_total_blocks;	/* 0x08 - total blocks */
    u_int64_t	cf_used_blocks;		/* 0x10 - used blocks */
    u_int32_t	cf_blockmap_offset;	/* 0x18 - blockmap offset */
    u_int32_t	cf_magic2;		/* 0x1c - magic2 */
} cf_header_t;				/* 0x20 - total size */

typedef struct change_file_context {
    cf_header_t	cfc_header;
    const sysdep_dispatch_t *cfc_sysdep;
    void	*cfc_fd;
    u_int64_t	*cfc_blockmap;
    u_int64_t	cfc_blocksize;
    u_int64_t	cfc_blockcount;
    u_int64_t	cfc_curpos;
    u_int32_t	cfc_crc_tab32[CRC_TABLE_LEN];
} cf_context_t;

typedef struct change_file_block_trailer {
    u_int64_t	cfb_curblock;
    u_int32_t	cfb_crc;
    u_int32_t	cfb_magic;
} cf_block_trailer_t;
#endif	/* _CHANGEFILEINT_H_ */
