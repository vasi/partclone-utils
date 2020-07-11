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
    uint32_t	cf_magic;		/* 0x00 - magic */
    uint16_t	cf_version;		/* 0x04 - version */
    uint16_t	cf_flags;		/* 0x06 - flags */
    uint64_t	cf_total_blocks;	/* 0x08 - total blocks */
    uint64_t	cf_used_blocks;		/* 0x10 - used blocks */
    uint32_t	cf_blockmap_offset;	/* 0x18 - blockmap offset */
    uint32_t	cf_magic2;		/* 0x1c - magic2 */
} cf_header_t;				/* 0x20 - total size */

typedef struct change_file_context {
    cf_header_t	cfc_header;
    const sysdep_dispatch_t *cfc_sysdep;
    void	*cfc_fd;
    uint64_t	*cfc_blockmap;
    uint64_t	cfc_blocksize;
    uint64_t	cfc_blockcount;
    uint64_t	cfc_curpos;
    uint32_t	cfc_crc_tab32[CRC_TABLE_LEN];
} cf_context_t;

typedef struct change_file_block_trailer {
    uint64_t	cfb_curblock;
    uint32_t	cfb_crc;
    uint32_t	cfb_magic;
} cf_block_trailer_t;
#endif	/* _CHANGEFILEINT_H_ */
