/*
 * changefile.h	- Interface to the changefile library.
 */
/*
 * Copyright (c) 2011, Ideal World, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#ifndef	_CHANGEFILE_H_
#define	_CHANGEFILE_H_	1
#include <sys/types.h>
#include "sysdep_int.h"
int cf_init(const char *, const sysdep_dispatch_t *, u_int64_t, u_int64_t,
	    void **);
int cf_create(const char *, const sysdep_dispatch_t *, u_int64_t, u_int64_t,
	      void **);
int cf_verify(void *);
int cf_sync(void *);
int cf_finish(void *);
int cf_seek(void *, u_int64_t);
int cf_readblock(void *, void *);
int cf_blockused(void *);
int cf_writeblock(void *, void *);
#endif	/* _CHANGEFILE_H_ */
