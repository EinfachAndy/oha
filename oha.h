#ifndef OHA_ORDERED_HASHING_H_
#define OHA_ORDERED_HASHING_H_

#ifdef __cplusplus
extern "C" {
#define restrict
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef OHA_INLINE_ALL
#define OHA_PUBLIC_API static inline __attribute__((unused))
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
    double max_load_factor;
    struct oha_memory_fp memory;
    size_t key_size;
    size_t value_size;
    uint32_t max_elems;
    bool resizable;
};

struct oha_lpht_status {
    uint32_t max_elems;
    uint32_t elems_in_use;
    size_t size_in_bytes;
    double current_load_factor;
};

OHA_PUBLIC_API struct oha_lpht *
oha_lpht_create(const struct oha_lpht_config * config);
OHA_PUBLIC_API void
oha_lpht_destroy(struct oha_lpht * table);
__attribute__((pure)) OHA_PUBLIC_API void *
oha_lpht_look_up(const struct oha_lpht * table, const void * key);
OHA_PUBLIC_API void *
oha_lpht_insert(struct oha_lpht * table, const void * key);
__attribute__((pure)) OHA_PUBLIC_API void *
oha_lpht_get_key_from_value(const void * value);
OHA_PUBLIC_API void *
oha_lpht_remove(struct oha_lpht * table, const void * key);
OHA_PUBLIC_API int
oha_lpht_get_status(const struct oha_lpht * table, struct oha_lpht_status * status);
OHA_PUBLIC_API int
oha_lpht_reserve(struct oha_lpht * table, size_t elements);
OHA_PUBLIC_API int
oha_lpht_clear(struct oha_lpht * table);
OHA_PUBLIC_API int
oha_lpht_get_next_element_to_remove(struct oha_lpht * table, struct oha_key_value_pair * pair);

/**********************************************************************************************************************
 *  binary heap (bh)
 *
 *      - TODO add docu
 *
 **********************************************************************************************************************/
struct oha_bh_config {
    struct oha_memory_fp memory;
    size_t value_size;
    uint32_t max_elems;
    bool resizable;
};
#define OHA_BH_NOT_FOUND INT64_MIN
#define OHA_BH_MIN_VALUE (OHA_BH_NOT_FOUND + 1)
struct oha_bh;

OHA_PUBLIC_API struct oha_bh *
oha_bh_create(const struct oha_bh_config * config);
OHA_PUBLIC_API void
oha_bh_destroy(struct oha_bh * heap);
__attribute__((pure)) OHA_PUBLIC_API int64_t
oha_bh_find_min(const struct oha_bh * heap);
OHA_PUBLIC_API void *
oha_bh_delete_min(struct oha_bh * heap);
OHA_PUBLIC_API void *
oha_bh_insert(struct oha_bh * heap, int64_t key);
OHA_PUBLIC_API int64_t
oha_bh_change_key(struct oha_bh * heap, void * value, int64_t new_val);
OHA_PUBLIC_API void *
oha_bh_remove(struct oha_bh * const heap, void * const value, int64_t * out_key);

/**********************************************************************************************************************
 *  tpht (temporal prioritized hash table)
 *
 *      - TODO add docu
 *
 **********************************************************************************************************************/
struct oha_tpht;
struct oha_tpht_config {
    struct oha_lpht_config lpht_config;
};

OHA_PUBLIC_API struct oha_tpht *
oha_tpht_create(const struct oha_tpht_config * config);
OHA_PUBLIC_API void
oha_tpht_destroy(struct oha_tpht * tpht);
OHA_PUBLIC_API int
oha_tpht_increase_global_time(struct oha_tpht * tpht, int64_t timestamp);
OHA_PUBLIC_API void *
oha_tpht_insert(struct oha_tpht * tpht, const void * key, uint8_t timeout_slot_id);
__attribute__((pure)) OHA_PUBLIC_API void *
oha_tpht_look_up(const struct oha_tpht * tpht, const void * key);
OHA_PUBLIC_API void *
oha_tpht_remove(struct oha_tpht * tpht, const void * key);
OHA_PUBLIC_API int8_t
oha_tpht_add_timeout_slot(struct oha_tpht * tpht, int64_t timeout, uint32_t num_elements, bool resizable);
OHA_PUBLIC_API void *
oha_tpht_set_timeout_slot(struct oha_tpht * tpht, const void * key, uint8_t timeout_slot_id);
OHA_PUBLIC_API size_t
oha_tpht_next_timeout_entries(struct oha_tpht * tpht, struct oha_key_value_pair * next_pair, size_t num_pairs);
OHA_PUBLIC_API void *
oha_tpht_update_time_for_entry(struct oha_tpht * tpht, const void * key, int64_t new_timestamp);

// include all code as static inline functions
#ifdef OHA_INLINE_ALL
// include order must be done in this order!
#include "oha_bh_impl.h"
#include "oha_lpht_impl.h"
#include "oha_tpht_impl.h"
#endif

#ifdef __cplusplus
#undef restrict
}
#endif

#endif
