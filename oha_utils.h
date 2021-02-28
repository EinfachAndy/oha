#ifndef OHA_UTILS_H_
#define OHA_UTILS_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

#define OHA_ALIGN_UP(_num) (((_num) + ((SIZE_T_WIDTH) - 1)) & ~((SIZE_T_WIDTH) - 1))

__attribute__((always_inline))
static inline void * oha_move_ptr_num_bytes(const void * const ptr, size_t num_bytes)
{
    return (((uint8_t *)ptr) + num_bytes);
}

__attribute__((always_inline))
static inline void oha_free(const struct oha_memory_fp * const memory, void * const ptr)
{
    assert(memory);
    if (memory->free != NULL) {
        memory->free(ptr, memory->alloc_user_ptr);

    } else {
        free(ptr);
    }
}

__attribute__((always_inline))
static inline void * oha_calloc(const struct oha_memory_fp * const memory, size_t size)
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

__attribute__((always_inline))
static inline void * oha_malloc(const struct oha_memory_fp * const memory, size_t size)
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

__attribute__((always_inline))
static inline void * oha_realloc(const struct oha_memory_fp * const memory, void * const ptr, size_t size)
{
    assert(memory);

    if (ptr == NULL) {
        return oha_malloc(memory, size);
    }

    if (size == 0) {
        oha_free(memory, ptr);
        return NULL;
    }

    if (memory->realloc != NULL) {
        return memory->realloc(ptr, size, memory->alloc_user_ptr);
    } else {
        return realloc(ptr, size);
    }
}

/**
 * creates a modulo without division
 *  - modulo could takes up to 20 cycles
 *  - multiplication takes about 3 cycles
 */
__attribute__((always_inline))
static inline uint32_t oha_map_range_u32(uint32_t word, uint32_t p)
{
    return (uint32_t)(((uint64_t)word * (uint64_t)p) >> 32);
}

__attribute__((always_inline))
static inline bool oha_add_entry_to_array(const struct oha_memory_fp * const memory,
                                          void ** const array,
                                          size_t entry_size,
                                          size_t * const entry_count)
{
    assert(memory);
    if (array == NULL || entry_count == NULL) {
        return false;
    }

    if (entry_size == 0) {
        return false;
    }

    if (*array == NULL) {
        *entry_count = 0;
    }

    uint8_t * new_memory = oha_realloc(memory, *array, entry_size * ((*entry_count) + 1));
    if (new_memory != NULL) {
        (*entry_count)++;
        *array = new_memory;
        return true;
    }

    return false;
}

__attribute__((always_inline))
static inline bool oha_remove_entry_from_array(const struct oha_memory_fp * const memory,
                                               void ** const array,
                                               size_t entry_size,
                                               size_t * const entry_count,
                                               size_t entry_index)
{
    assert(memory);
    if (array == NULL || entry_count == NULL || entry_index >= (*entry_count)) {
        return false;
    }

    if (*array == NULL || *entry_count == 0) {
        return false;
    }

    if (entry_index < (*entry_count) - 1) {
        memcpy(((uint8_t *)(*array)) + entry_index * entry_size,
               ((uint8_t *)(*array)) + ((*entry_count) - 1) * entry_size,
               entry_size);
    }

    void * new_memory = oha_realloc(memory, *array, entry_size * (*entry_count - 1));
    if (new_memory != *array) {
        *array = new_memory;
    }

    (*entry_count)--;

    return true;
}

#endif
