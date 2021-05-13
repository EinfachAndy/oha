#ifndef OHA_LINEAR_PROBING_HASH_TABLE_H_
#define OHA_LINEAR_PROBING_HASH_TABLE_H_

#include "oha.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "oha_utils.h"

#include "xxHash/xxh3.h"

#define OHA_LPHT_EMPTY_BUCKET (-1)

struct oha_lpht_key_bucket {
    uint32_t value_bucket_number;
    // TODO shrink psl size and introduce a index for the value buckets
    int32_t psl; // probe sequence length
    // key buffer is always aligned on 32 bit and 64 bit architectures
    uint8_t key_buffer[];
};

struct oha_lpht {
    struct oha_lpht_key_bucket * iter; // state of the iterator
    void * value_buckets;
    struct oha_lpht_key_bucket * key_buckets;
    struct oha_lpht_key_bucket * last_key_bucket;
    size_t key_bucket_size;   // size in bytes of one whole hash table key bucket, memory aligned
    size_t value_bucket_size; // size in bytes of one whole hash table value bucket, memory aligned
    uint32_t max_indicies;    // number of all allocated hash table buckets
    uint32_t elems;           // current number of inserted elements
    struct oha_lpht_config config;
};

OHA_PUBLIC_API void
oha_lpht_destroy_int(struct oha_lpht * const table);
OHA_PUBLIC_API void *
oha_lpht_insert_int(struct oha_lpht * const table, const void * const key);
OHA_PUBLIC_API int
oha_lpht_iter_init_int(struct oha_lpht * const table);
OHA_PUBLIC_API int
oha_lpht_iter_next_int(struct oha_lpht * const table, struct oha_key_value_pair * const pair);

__attribute__((always_inline)) static inline void
i_oha_lpht_clean_up(struct oha_lpht * const table)
{
    assert(table);
    const struct oha_memory_fp * memory = &table->config.memory;

    oha_free(memory, table->key_buckets);
    oha_free(memory, table->value_buckets);
    memset(table, 0, sizeof(*table));
}

__attribute__((always_inline)) static inline bool
i_oha_lpht_is_occupied(const struct oha_lpht_key_bucket * const bucket)
{
    return bucket->psl >= 0;
}

__attribute__((always_inline)) static inline void *
i_oha_lpht_get_value(const struct oha_lpht * const table, const struct oha_lpht_key_bucket * const bucket)
{
    return oha_move_ptr_num_bytes(table->value_buckets, table->value_bucket_size * bucket->value_bucket_number);
}

__attribute__((always_inline)) static inline uint32_t
i_oha_lpht_hash_key(const struct oha_lpht * const table, const void * const key)
{
    return XXH3_64bits(key, table->config.key_size);
}

__attribute__((always_inline)) static inline struct oha_lpht_key_bucket *
i_oha_lpht_get_start_bucket(const struct oha_lpht * const table, uint32_t hash)
{
    size_t index = oha_map_range_u32(hash, table->max_indicies);
    return oha_move_ptr_num_bytes(table->key_buckets, table->key_bucket_size * index);
}

__attribute__((always_inline)) static inline struct oha_lpht_key_bucket *
i_oha_lpht_get_next_bucket(const struct oha_lpht * const table, const struct oha_lpht_key_bucket * const bucket)
{
    struct oha_lpht_key_bucket * current = oha_move_ptr_num_bytes(bucket, table->key_bucket_size);
    // overflow, get to the first elem
    if (current > table->last_key_bucket) {
        current = table->key_buckets;
    }
    return current;
}

__attribute__((always_inline)) static inline int
i_oha_lpht_init_table(struct oha_lpht * const table)
{
    // TODO support zero value size as hash set
    if (table->config.max_elems == 0 || table->config.value_size == 0 || table->config.max_load_factor <= 0.0 ||
        table->config.max_load_factor >= 1.0) {
        return -1;
    }

    if (table->config.key_size == 0) {
        return -2;
    }

    // TODO add overflow checks
    table->max_indicies = ceil((1.0F / table->config.max_load_factor) * (float)table->config.max_elems) + 1.0F;
    table->value_bucket_size = OHA_ALIGN_UP(table->config.value_size);
    table->key_bucket_size = OHA_ALIGN_UP(sizeof(struct oha_lpht_key_bucket) + table->config.key_size);

    // allocate memory
    const struct oha_memory_fp * memory = &table->config.memory;
    table->key_buckets = oha_malloc(memory, table->key_bucket_size * table->max_indicies);
    if (table->key_buckets == NULL) {
        i_oha_lpht_clean_up(table);
        return -3;
    }

    table->value_buckets = oha_malloc(memory, table->value_bucket_size * table->max_indicies);
    if (table->value_buckets == NULL) {
        i_oha_lpht_clean_up(table);
        return -4;
    }

    table->last_key_bucket =
        oha_move_ptr_num_bytes(table->key_buckets, table->key_bucket_size * (table->max_indicies - 1));
    table->iter = NULL;

    // connect hash buckets and value buckets
    struct oha_lpht_key_bucket * current_key_bucket = table->key_buckets;
    void * current_value_bucket = table->value_buckets;
    for (size_t i = 0; i < table->max_indicies; i++) {
        current_key_bucket->value_bucket_number = i;
        current_key_bucket->psl = OHA_LPHT_EMPTY_BUCKET;
        current_key_bucket = oha_move_ptr_num_bytes(current_key_bucket, table->key_bucket_size);
        current_value_bucket = oha_move_ptr_num_bytes(current_value_bucket, table->value_bucket_size);
    }

    return 0;
}

__attribute__((always_inline)) static inline int
i_oha_lpht_resize(struct oha_lpht * const table, size_t max_elements)
{
    if (!table->config.resizable) {
        return -1;
    }

    if (max_elements <= table->config.max_elems) {
        // nothing todo
        return 0;
    }

    struct oha_lpht tmp_table;
    memset(&tmp_table, 0, sizeof(tmp_table));
    tmp_table.config = table->config;
    tmp_table.config.max_elems = max_elements;

    if (i_oha_lpht_init_table(&tmp_table) != 0) {
        return -3;
    }

    // copy elements
    struct oha_key_value_pair pair;
    for (oha_lpht_iter_init_int(table); oha_lpht_iter_next_int(table, &pair) == 0;) {
        assert(pair.key != NULL);
        assert(pair.value != NULL);
        // TODO reuse value buckets and do not reallocate it again
        void * new_value_buffer = oha_lpht_insert_int(&tmp_table, pair.key);
        if (new_value_buffer == NULL) {
            i_oha_lpht_clean_up(&tmp_table);
            return -4;
        }

        // TODO: maybe do not memcpy, let old buffer alive? -> add free value list
        memcpy(new_value_buffer, pair.value, tmp_table.value_bucket_size);
    }

    // destroy old table buffers
    i_oha_lpht_clean_up(table);

    // copy new table
    *table = tmp_table;

    return 0;
}

__attribute__((always_inline)) static inline int
i_oha_lpht_grow(struct oha_lpht * const table)
{
    return i_oha_lpht_resize(table, 2 * table->config.max_elems);
}

__attribute__((always_inline)) static inline void *
i_oha_lpht_robin_hood_emplace(struct oha_lpht * const table,
                              void const * const key,
                              int32_t psl,
                              struct oha_lpht_key_bucket * iter)
{
    if (table->elems >= table->config.max_elems) {
        // double size table
        if (i_oha_lpht_grow(table)) {
            return NULL;
        }
        return oha_lpht_insert_int(table, key);
    } else if (!i_oha_lpht_is_occupied(iter)) {
        // terminate robin hood insertion, we found a empty bucket
        memcpy(iter->key_buffer, key, table->config.key_size);
        iter->psl = psl;
        table->elems++;
        return i_oha_lpht_get_value(table, iter);
    }

    // 1. copy poor bucket to temporal memory slot
    char buffer[table->key_bucket_size];
    struct oha_lpht_key_bucket * tmp_key_bucket = (struct oha_lpht_key_bucket *)buffer;
    memcpy(tmp_key_bucket->key_buffer, key, table->config.key_size);
    tmp_key_bucket->value_bucket_number = iter->value_bucket_number;

    // swap poor and the rich bucket
    struct oha_lpht_key_bucket * const inserted_key_bucket = iter;
    i_oha_swap_memory(iter->key_buffer, tmp_key_bucket->key_buffer, table->config.key_size);
    OHA_SWAP(iter->psl, psl);

    for (++psl, iter = i_oha_lpht_get_next_bucket(table, iter);;
         ++psl, iter = i_oha_lpht_get_next_bucket(table, iter)) {
        if (!i_oha_lpht_is_occupied(iter)) {
            // terminate robin hood insertion, we found a empty bucket
            iter->psl = psl;
            memcpy(iter->key_buffer, tmp_key_bucket->key_buffer, table->config.key_size);
            OHA_SWAP(tmp_key_bucket->value_bucket_number, iter->value_bucket_number);
            table->elems++;
            inserted_key_bucket->value_bucket_number = tmp_key_bucket->value_bucket_number;
            return i_oha_lpht_get_value(table, inserted_key_bucket);
        } else if (psl > iter->psl) {
            // apply robin hood creed and swap the poor and the rich bucket
            i_oha_swap_memory(iter->key_buffer, tmp_key_bucket->key_buffer, table->config.key_size);
            OHA_SWAP(iter->psl, psl);
            OHA_SWAP(tmp_key_bucket->value_bucket_number, iter->value_bucket_number);
        }
    }
}

OHA_PUBLIC_API void
oha_lpht_destroy_int(struct oha_lpht * const table)
{
    assert(table);
    const struct oha_memory_fp * memory = &table->config.memory;

    i_oha_lpht_clean_up(table);
    oha_free(memory, table);
}

OHA_PUBLIC_API struct oha_lpht *
oha_lpht_create_int(const struct oha_lpht_config * const config)
{
    assert(config);
    struct oha_lpht * const table = oha_calloc(&config->memory, sizeof(struct oha_lpht));
    if (table == NULL) {
        return NULL;
    }

    table->config = *config;
    if (0 != i_oha_lpht_init_table(table)) {
        oha_free(&config->memory, table);
        return NULL;
    }
    return table;
}

// return pointer to value
OHA_PUBLIC_API struct oha_lpht_key_bucket *
oha_lpht_look_up_int(const struct oha_lpht * const table, const void * const key)
{
    assert(table);
    assert(key);
    uint32_t hash = i_oha_lpht_hash_key(table, key);
    // TODO add check for max prob counter

    struct oha_lpht_key_bucket * iter = i_oha_lpht_get_start_bucket(table, hash);
    for (int32_t psl = 0; psl <= iter->psl; iter = i_oha_lpht_get_next_bucket(table, iter), ++psl) {
        // circle + length check
        if (memcmp(iter->key_buffer, key, table->config.key_size) == 0) {
            return iter;
        }
    }
    return NULL;
}

// return pointer to value
OHA_PUBLIC_API void *
oha_lpht_insert_int(struct oha_lpht * const table, const void * const key)
{
    assert(table);
    assert(key);

    uint32_t hash = i_oha_lpht_hash_key(table, key);
    struct oha_lpht_key_bucket * iter = i_oha_lpht_get_start_bucket(table, hash);

    // do linear probing
    int32_t psl = 0;
    for (; psl <= iter->psl; iter = i_oha_lpht_get_next_bucket(table, iter), ++psl) {

        // found a already inserted element
        if (memcmp(iter->key_buffer, key, table->config.key_size) == 0) {
            // already inserted
            return i_oha_lpht_get_value(table, iter);
        }
    }

    // unfair, we need to apply the robin hood creed
    // the new key was definite not in the table, otherwise we already found it, because of
    // the robin hood invariant
    return i_oha_lpht_robin_hood_emplace(table, key, psl, iter);
}

OHA_PUBLIC_API int
oha_lpht_reserve_int(struct oha_lpht * const table, size_t elements)
{
    assert(table);

    bool save_resize_mode = table->config.resizable;
    table->config.resizable = true;
    int retval = i_oha_lpht_resize(table, elements);
    table->config.resizable = save_resize_mode;

    return retval;
}

OHA_PUBLIC_API int
oha_lpht_iter_init_int(struct oha_lpht * const table)
{
    assert(table);

    table->iter = table->key_buckets;
    return 0;
}

OHA_PUBLIC_API int
oha_lpht_iter_next_int(struct oha_lpht * const table, struct oha_key_value_pair * const pair)
{
    assert(table);
    assert(pair);

    if (table->iter == NULL) {
        // iterator was not initialised
        return -2;
    }

    bool stop = false;

    while (table->iter <= table->last_key_bucket) {
        if (i_oha_lpht_is_occupied(table->iter)) {
            pair->key = table->iter->key_buffer;
            pair->value = i_oha_lpht_get_value(table, table->iter);
            stop = true;
        }
        table->iter = oha_move_ptr_num_bytes(table->iter, table->key_bucket_size);
        if (stop == true) {
            break;
        }
    }

    return stop != true;
}

// return true if element was in the table
OHA_PUBLIC_API void *
oha_lpht_remove_int(struct oha_lpht * const table, const void * const key)
{
    assert(table && key);
    struct oha_lpht_key_bucket * bucket_to_remove = oha_lpht_look_up_int(table, key);
    if (bucket_to_remove == NULL) {
        return NULL;
    }

    uint32_t value_bucket_index = bucket_to_remove->value_bucket_number;

    // remove bucket
    bucket_to_remove->psl = OHA_LPHT_EMPTY_BUCKET;

    struct oha_lpht_key_bucket * iter = bucket_to_remove;
    struct oha_lpht_key_bucket * iter_next = i_oha_lpht_get_next_bucket(table, iter);
    while (iter_next->psl > 0) {

        // back shift and decrement psl
        memcpy(iter->key_buffer, iter_next->key_buffer, table->config.key_size);
        OHA_SWAP(iter->value_bucket_number, iter_next->value_bucket_number);
        iter->psl = iter_next->psl - 1;
        iter_next->psl = OHA_LPHT_EMPTY_BUCKET;

        iter = iter_next;
        iter_next = i_oha_lpht_get_next_bucket(table, iter);
    };

    table->elems--;

    return oha_move_ptr_num_bytes(table->value_buckets, table->value_bucket_size * value_bucket_index);
}

OHA_PUBLIC_API int
oha_lpht_get_status_int(const struct oha_lpht * const table, struct oha_lpht_status * const status)
{
    assert(table && status);

    status->max_elems = table->config.max_elems;
    status->elems_in_use = table->elems;
    status->size_in_bytes =
        // key buckets
        table->key_bucket_size * table->max_indicies +
        // value buckets
        table->value_bucket_size * table->max_indicies +
        // table offset size
        sizeof(struct oha_lpht);
    status->current_load_factor = (float)table->elems / (float)table->max_indicies;
    return 0;
}

/**********************************************************************************************************************
 *
 * public interface functions section
 *
 *********************************************************************************************************************/

OHA_PUBLIC_API void
oha_lpht_destroy(struct oha_lpht * const table)
{
#if OHA_NULL_POINTER_CHECKS
    if (table == NULL) {
        return;
    }
#endif
    oha_lpht_destroy_int(table);
}

OHA_PUBLIC_API struct oha_lpht *
oha_lpht_create(const struct oha_lpht_config * const config)
{
#if OHA_NULL_POINTER_CHECKS
    if (config == NULL) {
        return NULL;
    }
#endif
    return oha_lpht_create_int(config);
}

// return pointer to value
OHA_PUBLIC_API void *
oha_lpht_look_up(const struct oha_lpht * const table, const void * const key)
{
#if OHA_NULL_POINTER_CHECKS
    if (table == NULL || key == NULL) {
        return NULL;
    }
#endif
    struct oha_lpht_key_bucket * bucket = oha_lpht_look_up_int(table, key);
    if (bucket != NULL) {
        return i_oha_lpht_get_value(table, bucket);
    }
    // not found
    return NULL;
}

// return pointer to value
OHA_PUBLIC_API void *
oha_lpht_insert(struct oha_lpht * const table, const void * const key)
{
#if OHA_NULL_POINTER_CHECKS
    if (table == NULL || key == NULL) {
        return NULL;
    }
#endif
    return oha_lpht_insert_int(table, key);
}

OHA_PUBLIC_API int
oha_lpht_reserve(struct oha_lpht * const table, size_t elements)
{
#if OHA_NULL_POINTER_CHECKS
    if (table == NULL) {
        return -1;
    }
#endif
    return oha_lpht_reserve_int(table, elements);
}

OHA_PUBLIC_API int
oha_lpht_iter_init(struct oha_lpht * const table)
{
#if OHA_NULL_POINTER_CHECKS
    if (table == NULL) {
        return -1;
    }
#endif
    return oha_lpht_iter_init_int(table);
}

OHA_PUBLIC_API int
oha_lpht_iter_next(struct oha_lpht * const table, struct oha_key_value_pair * const pair)
{
#if OHA_NULL_POINTER_CHECKS
    if (table == NULL || pair == NULL) {
        return -1;
    }
#endif
    return oha_lpht_iter_next_int(table, pair);
}

// return true if element was in the table
OHA_PUBLIC_API void *
oha_lpht_remove(struct oha_lpht * const table, const void * const key)
{
#if OHA_NULL_POINTER_CHECKS
    if (table == NULL || key == NULL) {
        return NULL;
    }
#endif
    return oha_lpht_remove_int(table, key);
}

OHA_PUBLIC_API int
oha_lpht_get_status(const struct oha_lpht * const table, struct oha_lpht_status * const status)
{
#if OHA_NULL_POINTER_CHECKS
    if (table == NULL || status == NULL) {
        return -1;
    }
#endif
    return oha_lpht_get_status_int(table, status);
}

#endif
