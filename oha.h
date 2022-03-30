#ifndef OHA_ORDERED_HASHING_H_
#define OHA_ORDERED_HASHING_H_

#ifdef __cplusplus
extern "C" {
#define restrict
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OHA_FORCE_INLINE __attribute__((always_inline)) __attribute__((unused)) static inline
#define OHA_PRIVATE_API __attribute__((unused)) static

#ifdef OHA_INLINE_ALL
#define OHA_PUBLIC_API OHA_FORCE_INLINE
#else
#define OHA_PUBLIC_API
#endif

#ifdef OHA_DISABLE_NULL_POINTER_CHECKS
#define OHA_NULL_POINTER_CHECKS 0
#else
#define OHA_NULL_POINTER_CHECKS 1
#endif

struct oha_key_value_pair {
    void * key;
    void * value;
};

typedef void * (*oha_malloc_fp)(size_t size, void * user_ptr);
typedef void * (*oha_realloc_fp)(void * ptr, size_t size, void * user_ptr);
typedef void (*oha_free_fp)(void * ptr, void * user_ptr);

struct oha_memory_fp {
    oha_malloc_fp malloc;
    oha_realloc_fp realloc;
    oha_free_fp free;
    void * alloc_user_ptr;
};

/**********************************************************************************************************************
 *  linear probing hash table (lpht)
 *
 *      - TODO add docu
 *
 **********************************************************************************************************************/
struct oha_lpht;

struct oha_lpht_config {
    /*
     * The maximum number of elements that could placed in the table, this value is lower than the allocated
     * number of hash table buckets, because of performance reasons. The ratio is configurable via the load factor.
     */
    uint32_t max_elems;
    float max_load_factor;
    size_t key_size;
    size_t value_size;
    struct oha_memory_fp memory;
    bool resizable;
};

struct oha_lpht_status {
    uint32_t max_elems;
    uint32_t elems_in_use;
    size_t size_in_bytes;
    float current_load_factor;
};

OHA_PUBLIC_API struct oha_lpht *
oha_lpht_create(const struct oha_lpht_config * config);
OHA_PUBLIC_API void
oha_lpht_destroy(struct oha_lpht * table);
__attribute__((pure)) OHA_PUBLIC_API void *
oha_lpht_look_up(const struct oha_lpht * table, const void * key);
OHA_PUBLIC_API void *
oha_lpht_insert(struct oha_lpht * table, const void * key);
OHA_PUBLIC_API void *
oha_lpht_remove(struct oha_lpht * table, const void * key);
OHA_PUBLIC_API int
oha_lpht_get_status(const struct oha_lpht * table, struct oha_lpht_status * status);
OHA_PUBLIC_API int
oha_lpht_reserve(struct oha_lpht * table, uint32_t elements);
OHA_PUBLIC_API int
oha_lpht_iter_init(struct oha_lpht * table);
OHA_PUBLIC_API int
oha_lpht_iter_next(struct oha_lpht * table, struct oha_key_value_pair * pair);

// include all code as static inline functions
#ifdef OHA_INLINE_ALL
#include "oha_lpht_impl.h"
#endif

#ifdef __cplusplus
#undef restrict
}
#endif

#endif
