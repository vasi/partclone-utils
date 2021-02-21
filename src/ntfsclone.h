/**
 * ntfsclone.h - Part of Ntfsclone project.
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
#ifndef _NTFSCLONE_H_
#define _NTFSCLONE_H_ 1

/*
 * This is derived from ntfsprogs-2.0.0/ntfsprogs/ntfsclone.c
 */
#define IMAGE_MAGIC      "\0ntfsclone-image"
#define IMAGE_MAGIC_SIZE 16

/* This is the first endianness safe format version. */
#define NTFSCLONE_IMG_VER_MAJOR_ENDIANNESS_SAFE 10
#define NTFSCLONE_IMG_VER_MINOR_ENDIANNESS_SAFE 0

/*
 * Set the version to 10.0 to avoid colisions with old ntfsclone which
 * stupidly used the volume version as the image version...  )-:  I hope NTFS
 * never reaches version 10.0 and if it does one day I hope no-one is using
 * such an old ntfsclone by then...
 *
 * NOTE: Only bump the minor version if the image format and header are still
 * backwards compatible.  Otherwise always bump the major version.  If in
 * doubt, bump the major version.
 */
#define NTFSCLONE_IMG_VER_MAJOR 10
#define NTFSCLONE_IMG_VER_MINOR 0

/* All values are in little endian. */
typedef struct ntfs_image_header {
    char          magic[IMAGE_MAGIC_SIZE];
    unsigned char major_ver;
    unsigned char minor_ver;
    uint32_t      cluster_size;
    int64_t       device_size;
    int64_t       nr_clusters;
    int64_t       inuse;
    uint32_t      offset_to_image_data; /* From start of image_hdr. */
} __attribute__((__packed__)) image_hdr;

#define NTFSCLONE_IMG_HEADER_SIZE_OLD \
    (offsetof(typeof(image_hdr), offset_to_image_data))

#define NTFS_MBYTE (1000 * 1000)

#define ERR_PREFIX  "ERROR"
#define PERR_PREFIX ERR_PREFIX "(%d): "
#define NERR_PREFIX ERR_PREFIX ": "

#define LAST_METADATA_INODE 11

#define NTFS_MAX_CLUSTER_SIZE 65536
#define NTFS_SECTOR_SIZE      512

#define VERSION_SIZE 4

typedef struct ntfsclone_atom {
    char nca_atype;
    union {
        uint64_t      ncau_empty_count;
        unsigned char ncau_used_cluster[sizeof(uint64_t)];
    } nca_union;
} __attribute__((__packed__)) ntfsclone_atom_t;
#define ATOM_TO_DATA_OFFSET \
    ((uint64_t) & (((ntfsclone_atom_t *)NULL)->nca_union.ncau_used_cluster[0]))
#endif /* _NTFSCLONE_H_ */
