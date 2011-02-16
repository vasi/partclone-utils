/**
 * partclone.h - Part of Partclone project.
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * function and structure used by main.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef	_PARTCLONE_H_
#define	_PARTCLONE_H_	1
#define IMAGE_MAGIC "partclone-image"
#define IMAGE_MAGIC_SIZE 15

#define FS_MAGIC_SIZE 15
#define reiserfs_MAGIC "REISERFS"
#define reiser4_MAGIC "REISER4"
#define xfs_MAGIC "XFS"
#define extfs_MAGIC "EXTFS"
#define ext2_MAGIC "EXT2"
#define ext3_MAGIC "EXT3"
#define ext4_MAGIC "EXT4"
#define hfsplus_MAGIC "HFS Plus"
#define fat_MAGIC "FAT"
#define ntfs_MAGIC "NTFS"
#define ufs_MAGIC "UFS"
#define vmfs_MAGIC "VMFS"
#define jfs_MAGIC "JFS"
#define raw_MAGIC "raw"

#define IMAGE_VERSION "0001"
#define VERSION_SIZE 4
#define SECTOR_SIZE 512
#define CRC_SIZE 4

struct image_head 
{
    char magic[IMAGE_MAGIC_SIZE];
    char fs[FS_MAGIC_SIZE];
    char version[VERSION_SIZE];
    int block_size;
    unsigned long long device_size;
    unsigned long long totalblock;
    unsigned long long usedblocks;
    char buff[4096];
};
typedef struct image_head image_head;
#endif	/* _PARTCLONE_H_ */
