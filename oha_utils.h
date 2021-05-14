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

#define OHA_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define OMA_MIN(x, y) (((x) < (y)) ? (x) : (y))

#define OHA_ALIGN_UP(_num) (((_num) + ((SIZE_T_WIDTH)-1)) & ~((SIZE_T_WIDTH)-1))

#define OHA_SWAP(x, y)                                                                                                 \
    do {                                                                                                               \
        static_assert(sizeof(x) == sizeof(y), "swap of different types not supported");                                \
        unsigned char swap_temp[sizeof(x)];                                                                            \
        memcpy(swap_temp, &(y), sizeof(x));                                                                            \
        memcpy(&(y), &(x), sizeof(x));                                                                                 \
        memcpy(&(x), swap_temp, sizeof(x));                                                                            \
    } while (0)

OHA_FORCE_INLINE void
i_oha_swap_memory(void * const restrict a, void * const restrict b, const size_t size)
{
    char tmp_buffer[size];
    memcpy(tmp_buffer, a, size); // a -> tmp
    memcpy(a, b, size);          // b -> a
    memcpy(b, tmp_buffer, size); // tmp -> b
}

OHA_FORCE_INLINE void *
oha_move_ptr_num_bytes(const void * const ptr, size_t num_bytes)
{
    return (((uint8_t *)ptr) + num_bytes);
}

OHA_FORCE_INLINE void
oha_free(const struct oha_memory_fp * const memory, void * const ptr)
{
    assert(memory);
    if (memory->free != NULL) {
        memory->free(ptr, memory->alloc_user_ptr);

    } else {
        free(ptr);
    }
}

OHA_FORCE_INLINE void *
oha_calloc(const struct oha_memory_fp * const memory, size_t size)
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

OHA_FORCE_INLINE void *
oha_malloc(const struct oha_memory_fp * const memory, size_t size)
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

OHA_FORCE_INLINE void *
oha_realloc(const struct oha_memory_fp * const memory, void * const ptr, size_t size)
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

/*
 * fast compution of 2^x
 * see: https://stackoverflow.com/questions/466204/rounding-up-to-next-power-of-2
 */
OHA_FORCE_INLINE uint32_t
oha_next_power_of_two_32bit(uint32_t i)
{
    --i;
    i |= i >> 1;
    i |= i >> 2;
    i |= i >> 4;
    i |= i >> 8;
    i |= i >> 16;
    ++i;
    return i;
}

/*
 * fast compution of log2(x)
 * https://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers
 */
OHA_FORCE_INLINE int
oha_log2_32bit(uint32_t value)
{
    static const uint8_t tab32[32] = {0, 9,  1,  10, 13, 21, 2,  29, 11, 14, 16, 18, 22, 25, 3, 30,
                                      8, 12, 20, 28, 15, 17, 24, 7,  19, 27, 23, 6,  26, 5,  4, 31};

    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return tab32[(size_t)(value * 0x07C4ACDD) >> 27];
}

OHA_FORCE_INLINE uint32_t
oha_lpht_hash_32bit(const void * buffer, const size_t len)
{
    const uint32_t * b = buffer;
    uint32_t res = 2147483647; // magic prime
    uint32_t l = len >> 2;     // key must be at least 4 byte
    for (uint32_t i = 0; i < l; i++) {
        res += *b++;
    }
    return res;
}

/**
 * creates a modulo without division
 *  - modulo could takes up to 20 cycles
 *  - multiplication takes about 3 cycles
 */
OHA_FORCE_INLINE uint32_t
oha_map_range_u32(uint32_t word, uint32_t p)
{
    return (uint32_t)(((uint64_t)word * (uint64_t)p) >> 32);
}

OHA_FORCE_INLINE bool
oha_add_entry_to_array(const struct oha_memory_fp * const memory,
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

OHA_FORCE_INLINE bool
oha_remove_entry_from_array(const struct oha_memory_fp * const memory,
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
