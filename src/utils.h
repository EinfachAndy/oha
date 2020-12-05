#ifndef OHA_UTILS_H_
#define OHA_UTILS_H_

#include <stdint.h>
#include <stdlib.h>

#include "oha.h"

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

static inline void oha_free(const struct oha_memory_fp * memory, void * ptr)
{
    assert(memory);
    if (memory->free != NULL) {
        memory->free(ptr, memory->alloc_user_ptr);

    } else {
        free(ptr);
    }
}

static inline void * oha_calloc(const struct oha_memory_fp * memory, size_t size)
{
    assert(memory);
    void * ptr;
    if (memory->malloc != NULL) {
        ptr = memory->malloc(size, memory->alloc_user_ptr);
        if (ptr != NULL) {
            memset(ptr, 0, size);
        }
    } else {
        ptr = calloc(1, size);
    }
    return ptr;
}

static inline void * oha_malloc(const struct oha_memory_fp * memory, size_t size)
{
    assert(memory);
    void * ptr;
    if (memory->malloc != NULL) {
        ptr = memory->malloc(size, memory->alloc_user_ptr);
    } else {
        ptr = malloc(size);
    }
    return ptr;
}

static inline void * oha_realloc(const struct oha_memory_fp * memory, void * ptr, size_t size)
{
    assert(memory);
    if (memory->realloc != NULL) {
        return memory->realloc(ptr, size, memory->alloc_user_ptr);
    } else {
        return realloc(ptr, size);
    }
}

#endif
