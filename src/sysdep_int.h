/*
 * sysdep_int.h	- System-dependent interfaces.
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
#ifndef _SYSDEP_INT_H_
#define _SYSDEP_INT_H_ 1
#include <stdint.h>

typedef enum sysdep_open_mode {
    SYSDEP_OPEN_NONE = 0,
    SYSDEP_OPEN_RO   = 1,
    SYSDEP_OPEN_RW   = 2,
    SYSDEP_OPEN_WO   = 3,
    SYSDEP_CREATE    = 4
} sysdep_open_mode_t;

typedef enum sysdep_whence {
    SYSDEP_SEEK_ABSOLUTE = 0,
    SYSDEP_SEEK_RELATIVE = 1,
    SYSDEP_SEEK_END      = 2
} sysdep_whence_t;

typedef struct sysdep_dispatch {
    /*
     * sys_open		- Open a file handle and return a pointer to it.
     *
     * Parameters:
     *	rhp	- Pointer to where to store pointer.
     *	p	- Path to open.
     *	flags	- file control flags (see open(2))
     *	mode	- file mode (see open(2))
     *
     * Returns:
     * 	0	- Success
     *	ENOMEM	- No memory for handle.
     *	error	- Otherwise.
     */
    int (*sys_open)(void *rhp, const char *p, sysdep_open_mode_t mode);
    /*
     * sys_close	- Close a file handle and free pointer.
     *
     * Parameters:
     *	rh	- File handle
     *
     * Returns:
     *	0	- Success.
     *	EINVAL	- Invalid file handle.
     *	error	- Otherwise.
     */
    int (*sys_close)(void *rh);
    /*
     * sys_seek		- Seek to an offset in a file.
     *
     * Parameters:
     *	rh	- File handle.
     *	offset	- Offset to seek to.
     *	whence	- One of SYSDEP_SEEK_ABSOLUTE, SYSDEP_SEEK_RELATIVE or
     * 		  SYSDEP_SEEK_END.
     *	resoffp	- Pointer to resultant location (can be null).
     *
     * Returns:
     *	0	- Success.
     *	EINVAL	- Invalid file handle.
     *	error	- Otherwise.
     */
    int (*sys_seek)(void *rh, int64_t offset, sysdep_whence_t whence,
                    uint64_t *resoffp);
    /*
     * sys_read		- Read data from the current offset.
     *
     * Parameters:
     *	rh	- File handle.
     *	buf	- Buffer to read into.
     *	len	- Length to read.
     *	nr	- How many bytes read (written on success).
     *
     * Returns:
     *	0	- Success.
     *	EINVAL	- Invalid file handle.
     *	error	- Otherwise.
     */
    int (*sys_read)(void *rh, void *buf, uint64_t len, uint64_t *nr);
    /*
     * sys_write	- Write data at the current offset.
     *
     * Parameters:
     *	rh	- File handle.
     *	buf	- Buffer to read into.
     *	len	- Length to read.
     *	nw	- How many bytes written (written on success).
     *
     * Returns:
     *	0	- Success.
     *	EINVAL	- Invalid file handle.
     *	error	- Otherwise.
     */
    int (*sys_write)(void *rh, void *buf, uint64_t len, uint64_t *nw);
    /*
     * sys_malloc	- Allocate dynamic memory.
     *
     * Parameters:
     *	nmpp	- Pointer to new memory pointer (written always).
     *	nbytes	- Size of memory to allocate.
     *
     * Returns:
     *	0	- Success
     *	EINVAL	- Invalid nmpp
     *	ENOMEM	- No memory available
     */
    int (*sys_malloc)(void *nmpp, uint64_t nbytes);
    /*
     * sys_free		- Free dynamic memory.
     *
     * Parameters:
     *	mp	- Pointer to memory.
     *
     * Returns:
     *	0	- Success
     *	EINVAL	- Invalid pointer
     */
    int (*sys_free)(void *mp);
    /*
     * sys_file_size	- Determine a file's size.
     *
     * Paramters:
     *  rh	- Open file handle.
     *  nbytes	- File size.
     */
    int (*sys_file_size)(void *rh, uint64_t *nbytes);
} sysdep_dispatch_t;
#endif /* _SYSDEP_INT_H_ */
