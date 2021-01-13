/*
 * POSIX 1003.2 checksum
 */

#ifndef _CRC_POSIX1003_2_H_
#define _CRC_POSIX1003_2_H_

#include <linux/types.h>

uint32_t crc_p1003d2(uint8_t *psrc, int clen);

#endif /* _CRC_POSIX1003_2_H_ */
