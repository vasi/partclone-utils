/*
 * libchecksum.c - implementation to checksum algorithms
 */
#include "libchecksum.h"

#define CRC32_SEED      0xFFFFFFFFL
#define CRC32_TABLE_LEN 256

static uint32_t crc_tab32[CRC32_TABLE_LEN] = {0};

/**
 * Initialise crc32 lookup table if it is not already done and initialise the
 * seed to the default implementation seed value
 */
crc32_t
init_crc32() {
    if (crc_tab32[0] == 0) {
        crc32_t  init_crc;
        uint32_t i, j;

        for (i = 0; i < CRC32_TABLE_LEN; ++i) {
            init_crc = i;
            for (j = 0; j < 8; j++) {
                if (init_crc & 0x00000001L)
                    init_crc = (init_crc >> 1) ^ 0xEDB88320L;
                else
                    init_crc = init_crc >> 1;
            }

            crc_tab32[i] = init_crc;
        }
    }

    return CRC32_SEED;
}

/// the crc32 function, reference from libcrc.
/// Author is Lammert Bies  1999-2007
/// Mail: info@lammertbies.nl
/// http://www.lammertbies.nl/comm/info/nl_crc-calculation.html
/// generate crc32 code
crc32_t
update_crc32(crc32_t seed, void *buffer, uint64_t size) {

    uint8_t const *      buf = (uint8_t *)buffer;
    uint8_t const *const end = buf + size;
    crc32_t              crc = seed;
    int32_t              tmp, long_c;

    while (buf != end) {
        long_c = *(buf++);
        tmp    = crc ^ long_c;
        crc    = (crc >> 8) ^ crc_tab32[tmp & 0xff];
    };

    return crc;
}
