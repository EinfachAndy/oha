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

#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
#error "currently not implemented"
struct oha_lpht_key_bucket;
struct oha_lpht_value_bucket {
    struct oha_lpht_key_bucket * key;
    uint8_t value_buffer[];
};
#define OHA_LPHT_VALUE_BUCKET_TYPE struct oha_lpht_value_bucket
#define OHA_LPHT_VALUE_BUCKET_SIZE sizeof(OHA_LPHT_VALUE_BUCKET_TYPE)
#else
#define OHA_LPHT_VALUE_BUCKET_TYPE void
#define OHA_LPHT_VALUE_BUCKET_SIZE 0
#endif

#define OHA_LPHT_EMPTY_BUCKET (UINT32_MAX)

struct oha_lpht_key_bucket {
    uint32_t value_bucket_number;
    // TODO shrink psl size and introduce a index for the value buckets
    uint32_t psl; // probe sequence length
    // key buffer is always aligned on 32 bit and 64 bit architectures
    uint8_t key_buffer[];
};

struct storage_info {
    size_t key_bucket_size;        // size in bytes of one whole hash table key bucket, memory aligned
    size_t hash_table_size_keys;   // size in bytes of the hole hash table keys
    size_t hash_table_size_values; // size in bytes of the hole hash table values
    uint32_t max_indicies;         // number of all allocated hash table buckets
};

struct oha_lpht {
    OHA_LPHT_VALUE_BUCKET_TYPE * value_buckets;
    struct oha_lpht_key_bucket * key_buckets;
    struct oha_lpht_key_bucket * last_key_bucket;
    struct oha_lpht_key_bucket * iter; // state of the iterator
    struct storage_info storage;
    struct oha_lpht_config config;
    uint32_t elems; // current number of inserted elements

    /*
     * the current maximal seen psl over all inserted buckets. This value gets only raised or set to zero by a rehash
     */
    uint32_t max_psl;
};

OHA_PUBLIC_API void
oha_lpht_destroy_int(struct oha_lpht * const table);
OHA_PUBLIC_API void *
oha_lpht_insert_int(struct oha_lpht * const table, const void * const key);
OHA_PUBLIC_API int
oha_lpht_iter_init_int(struct oha_lpht * const table);
OHA_PUBLIC_API int
oha_lpht_iter_next_int(struct oha_lpht * const table, struct oha_key_value_pair * const pair);

__attribute__((always_inline)) static inline bool
i_oha_lpht_is_occupied(const struct oha_lpht_key_bucket * const bucket)
{
    return bucket->psl != OHA_LPHT_EMPTY_BUCKET;
}

__attribute__((always_inline)) static inline void *
i_oha_lpht_get_value(const struct oha_lpht * const table, const struct oha_lpht_key_bucket * const bucket)
{
    return oha_move_ptr_num_bytes(table->value_buckets, table->config.value_size * bucket->value_bucket_number);
}

__attribute__((always_inline)) static inline uint32_t
i_oha_lpht_hash_key(const struct oha_lpht * const table, const void * const key)
{
    return XXH3_64bits(key, table->config.key_size);
}

__attribute__((always_inline)) static inline struct oha_lpht_key_bucket *
i_oha_lpht_get_start_bucket(const struct oha_lpht * const table, uint32_t hash)
{
    size_t index = oha_map_range_u32(hash, table->storage.max_indicies);
    return oha_move_ptr_num_bytes(table->key_buckets, table->storage.key_bucket_size * index);
}

__attribute__((always_inline)) static inline struct oha_lpht_key_bucket *
i_oha_lpht_get_next_bucket(const struct oha_lpht * const table, const struct oha_lpht_key_bucket * const bucket)
{
    struct oha_lpht_key_bucket * current = oha_move_ptr_num_bytes(bucket, table->storage.key_bucket_size);
    // overflow, get to the first elem
    if (OHA_UNLIKELY(current > table->last_key_bucket)) {
        current = table->key_buckets;
    }
    return current;
}

__attribute__((always_inline)) static inline void
i_oha_lpht_swap_bucket_values(struct oha_lpht_key_bucket * const restrict a,
                              struct oha_lpht_key_bucket * const restrict b)
{
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
    OHA_LPHT_VALUE_BUCKET_TYPE * value_a = i_oha_lpht_get_value(table, a);
    OHA_LPHT_VALUE_BUCKET_TYPE * value_b = i_oha_lpht_get_value(table, b);
    value_a->key = b;
    value_b->key = a;
#endif
    uint32_t tmp = a->value_bucket_number;
    a->value_bucket_number = b->value_bucket_number;
    b->value_bucket_number = tmp;
}

__attribute__((always_inline)) static inline int
i_oha_lpht_calculate_configuration(struct oha_lpht_config * const config, struct storage_info * const values)
{
    assert(config && values);

    // TODO support zero value size as hash set
    if (config->max_elems == 0 || config->value_size == 0 || config->max_load_factor <= 0.0 ||
        config->max_load_factor >= 1.0) {
        return -2;
    }

    if (config->key_size == 0) {
        return -3;
    }

    // TODO add overflow checks
    values->max_indicies = ceil((1 / config->max_load_factor) * config->max_elems) + 1;
    config->value_size = OHA_ALIGN_UP(config->value_size);
    values->key_bucket_size = OHA_ALIGN_UP(sizeof(struct oha_lpht_key_bucket) + config->key_size);
    values->hash_table_size_keys = values->key_bucket_size * values->max_indicies;
    values->hash_table_size_values = (config->value_size + OHA_LPHT_VALUE_BUCKET_SIZE) * values->max_indicies;

    return 0;
}

__attribute__((always_inline)) static inline struct oha_lpht *
i_oha_lpht_init_table(const struct storage_info * const storage, struct oha_lpht * const table)
{
    table->storage = *storage;
    const struct oha_memory_fp * memory = &table->config.memory;
    table->key_buckets = oha_malloc(memory, storage->hash_table_size_keys);
    if (table->key_buckets == NULL) {
        oha_lpht_destroy_int(table);
        return NULL;
    }

    table->value_buckets = oha_malloc(memory, storage->hash_table_size_values);
    if (table->value_buckets == NULL) {
        oha_lpht_destroy_int(table);
        return NULL;
    }

    table->last_key_bucket =
        oha_move_ptr_num_bytes(table->key_buckets, table->storage.key_bucket_size * (table->storage.max_indicies - 1));
    table->iter = NULL;

    // connect hash buckets and value buckets
    struct oha_lpht_key_bucket * current_key_bucket = table->key_buckets;
    OHA_LPHT_VALUE_BUCKET_TYPE * current_value_bucket = table->value_buckets;
    for (size_t i = 0; i < table->storage.max_indicies; i++) {
        current_key_bucket->value_bucket_number = i;
        current_key_bucket->psl = OHA_LPHT_EMPTY_BUCKET;
        current_key_bucket = oha_move_ptr_num_bytes(current_key_bucket, table->storage.key_bucket_size);
        current_value_bucket = oha_move_ptr_num_bytes(current_value_bucket, table->config.value_size);
    }

    return table;
}

__attribute__((always_inline)) static inline uint32_t
i_oha_lpht_robin_hood_emplace(struct oha_lpht * const table,
                              uint32_t psl,
                              void * key,
                              uint32_t value_bucket_number,
                              struct oha_lpht_key_bucket * iter)
{
    for (;;) {
        psl++;
        iter = i_oha_lpht_get_next_bucket(table, iter);
        if (!i_oha_lpht_is_occupied(iter)) {
            // terminate robin hood insertion, we found a empty bucket
            table->max_psl = OHA_MAX(table->max_psl, psl);
            iter->psl = psl;
            memcpy(iter->key_buffer, key, table->config.key_size);
            OHA_SWAP(value_bucket_number, iter->value_bucket_number);
            return value_bucket_number;
        } else if (psl > iter->psl) {
            table->max_psl = OHA_MAX(table->max_psl, psl);
            // apply robin hood creed and swap buckets
            i_oha_swap_memory(iter->key_buffer, key, table->config.key_size);
            OHA_SWAP(iter->psl, psl);
            OHA_SWAP(value_bucket_number, iter->value_bucket_number);
        }
    }
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

    const struct oha_memory_fp * memory = &table->config.memory;
    struct oha_lpht * tmp_table = oha_calloc(memory, sizeof(struct oha_lpht));
    if (tmp_table == NULL) {
        return -2;
    }

    tmp_table->config = table->config;
    tmp_table->config.max_elems = max_elements;

    struct storage_info storage;
    if (i_oha_lpht_calculate_configuration(&tmp_table->config, &storage)) {
        return -3;
    }

    if (i_oha_lpht_init_table(&storage, tmp_table) == NULL) {
        return -4;
    }

    // copy elements
    oha_lpht_iter_init_int(table);
    struct oha_key_value_pair pair;
    oha_lpht_iter_init_int(table);
    while (oha_lpht_iter_next_int(table, &pair) == 0) {
        assert(pair.key != NULL);
        assert(pair.value != NULL);
        // TODO reuse value buckets and do not reallocate it again
        void * new_value_buffer = oha_lpht_insert_int(tmp_table, pair.key);
        if (new_value_buffer == NULL) {
            oha_lpht_destroy_int(tmp_table);
            return -5;
        }

        // TODO: maybe do not memcpy, let old buffer alive? -> add free value list
        memcpy(new_value_buffer, pair.value, tmp_table->config.value_size);
    }

    // destroy old table buffers
    oha_free(&table->config.memory, table->key_buckets);
    oha_free(&table->config.memory, table->value_buckets);

    // copy new table
    *table = *tmp_table;

    oha_free(&table->config.memory, tmp_table);

    return 0;
}

OHA_PUBLIC_API void
oha_lpht_destroy_int(struct oha_lpht * const table)
{
    assert(table);
    const struct oha_memory_fp * memory = &table->config.memory;

    // fprintf(stderr, "max psl=%u\n", table->max_psl);

    oha_free(memory, table->key_buckets);
    oha_free(memory, table->value_buckets);
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
    struct storage_info storage;
    if (i_oha_lpht_calculate_configuration(&table->config, &storage)) {
        oha_free(&config->memory, table);
        return NULL;
    }

    return i_oha_lpht_init_table(&storage, table);
}

// return pointer to value
OHA_PUBLIC_API struct oha_lpht_key_bucket *
oha_lpht_look_up_int(const struct oha_lpht * const table, const void * const key)
{
    assert(table);
    assert(key);
    uint32_t hash = i_oha_lpht_hash_key(table, key);
    // TODO add check for max prob counter
    for (struct oha_lpht_key_bucket * iter = i_oha_lpht_get_start_bucket(table, hash); iter->psl <= table->max_psl + 1;
         iter = i_oha_lpht_get_next_bucket(table, iter)) {
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
    if (OHA_UNLIKELY(table->elems >= table->config.max_elems)) {
        struct oha_lpht_key_bucket * bucket = oha_lpht_look_up_int(table, key);
        if (bucket != NULL) {
            return i_oha_lpht_get_value(table, bucket);
        }

        // double size table
        if (i_oha_lpht_resize(table, 2 * table->config.max_elems)) {
            return NULL;
        }
    }

    uint32_t hash = i_oha_lpht_hash_key(table, key);
    struct oha_lpht_key_bucket * iter = i_oha_lpht_get_start_bucket(table, hash);

    // do linear probing
    for (uint32_t psl = 0;; psl++, iter = i_oha_lpht_get_next_bucket(table, iter)) {

        // simple insert
        if (!i_oha_lpht_is_occupied(iter)) {
            memcpy(iter->key_buffer, key, table->config.key_size);
            iter->psl = psl;
            table->max_psl = OHA_MAX(table->max_psl, psl);
            break;
        }

        // found a already inserted element
        if (memcmp(iter->key_buffer, key, table->config.key_size) == 0) {
            // already inserted
            return i_oha_lpht_get_value(table, iter);
        }

        if (psl > iter->psl) {
            // unfair, we need to apply the robin hood creed
            // the new key was definite not in the table, otherwise we already found it, because of
            //  the robin hood creed

            // 1. copy rich bucket to temporal memory slot
            const size_t key_kize = table->config.key_size;
            char displaced_key[key_kize];
            memcpy(displaced_key, iter->key_buffer, key_kize);
            const uint32_t next_free_value_number = iter->value_bucket_number;

            // 2. insert key
            OHA_SWAP(psl, iter->psl);
            memcpy(iter->key_buffer, key, key_kize);

            iter->value_bucket_number =
                i_oha_lpht_robin_hood_emplace(table, psl, displaced_key, next_free_value_number, iter);
            break;
        }
    }

    table->elems++;
    return i_oha_lpht_get_value(table, iter);
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

OHA_PUBLIC_API void *
oha_lpht_get_key_from_value_int(const void * const value)
{
    assert(value);
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
    OHA_LPHT_VALUE_BUCKET_TYPE * value_bucket =
        (OHA_LPHT_VALUE_BUCKET_TYPE *)((uint8_t *)value - offsetof(OHA_LPHT_VALUE_BUCKET_TYPE, value_buffer));
    assert(value_bucket->key->value == value_bucket);
    return value_bucket->key;
#else
    (void)value;
    return NULL;
#endif
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

    while (OHA_LIKELY(table->iter <= table->last_key_bucket)) {
        if (i_oha_lpht_is_occupied(table->iter)) {
            pair->key = table->iter->key_buffer;
            pair->value = i_oha_lpht_get_value(table, table->iter);
            stop = true;
        }
        table->iter = oha_move_ptr_num_bytes(table->iter, table->storage.key_bucket_size);
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
    const bool do_back_shifting = true;
    while (do_back_shifting) {

        if (!i_oha_lpht_is_occupied(iter_next)) {
            // finished, we found a empty bucket
            break;
        }

        if (iter_next->psl == 0) {
            // finished, we found a bucket switch has a optimal position
            break;
        }

        // back shift and decrement psl
        memcpy(iter->key_buffer, iter_next->key_buffer, table->config.key_size);
        i_oha_lpht_swap_bucket_values(iter, iter_next);
        iter->psl = iter_next->psl - 1;
        iter_next->psl = OHA_LPHT_EMPTY_BUCKET;

        iter = iter_next;
        iter_next = i_oha_lpht_get_next_bucket(table, iter);
    };

    table->elems--;
    return oha_move_ptr_num_bytes(table->value_buckets, table->config.value_size * value_bucket_index);
}

OHA_PUBLIC_API int
oha_lpht_get_status_int(const struct oha_lpht * const table, struct oha_lpht_status * const status)
{
    assert(table && status);

    status->max_elems = table->config.max_elems;
    status->elems_in_use = table->elems;
    status->size_in_bytes =
        table->storage.hash_table_size_keys + table->storage.hash_table_size_values + sizeof(struct oha_lpht);
    status->current_load_factor = (float)table->elems / (float)table->storage.max_indicies;
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

OHA_PUBLIC_API void *
oha_lpht_get_key_from_value(const void * const value)
{
#if OHA_NULL_POINTER_CHECKS
    if (value == NULL) {
        return NULL;
    }
#endif
    return oha_lpht_get_key_from_value_int(value);
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
