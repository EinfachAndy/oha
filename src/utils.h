#ifndef OHA_UTILS_H_
#define OHA_UTILS_H_

#include <stdint.h>
#include <stdlib.h>

#if SIZE_MAX == (18446744073709551615UL)
#define SIZE_T_WIDTH 8
#elif SIZE_MAX == (4294967295UL)
#define SIZE_T_WIDTH 4
#else
#error "unsupported plattform"
#endif

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static inline size_t add_alignment(size_t unaligned_size)
{
    return unaligned_size + (unaligned_size % SIZE_T_WIDTH);
}

static inline void * move_ptr_num_bytes(void * ptr, size_t num_bytes)
{
    return (((uint8_t *)ptr) + num_bytes);
}

#endif
