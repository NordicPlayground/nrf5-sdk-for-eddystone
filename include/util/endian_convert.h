#ifndef ENDIAN_CONVERT_H
#define ENDIAN_CONVERT_H

#include <stdint.h>

#define BYTES_SWAP_16BIT(x) (x << 8 | x >> 8)
#define BYTES_REVERSE_32BIT(x) (x << 24 | (x << 8) & 0x00FF0000 | (x >> 8) & 0x0000FF00 | x >> 24)

#endif /*ENDIAN_CONVERT_H*/
