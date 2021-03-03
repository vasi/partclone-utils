/*
 * libchecksum.h - interfaces to checksum algorithms
 */

#ifndef _PU_LIBCHECKSUM_H_
#define _PU_LIBCHECKSUM_H_ 1

#include <stdint.h>

typedef uint32_t crc32_t;

crc32_t init_crc32();
crc32_t update_crc32(crc32_t seed, void *buf, uint64_t size);

#endif /* _PU_LIBCHECKSUM_H_ */
