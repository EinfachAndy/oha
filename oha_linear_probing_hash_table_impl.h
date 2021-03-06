#ifndef OHA_LINEAR_PROBING_HASH_TABLE_H_
#define OHA_LINEAR_PROBING_HASH_TABLE_H_

#include "oha.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "oha_utils.h"

// xxhash has most speed with O3
#pragma GCC push_options
#pragma GCC optimize("O3")
#include "xxHash/xxh3.h"
#pragma GCC pop_options

#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
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

struct oha_lpht_key_bucket {
    OHA_LPHT_VALUE_BUCKET_TYPE * value;
    uint32_t offset;
    uint32_t is_occupied; // only one bit in usage, could be extend for future states
    // key buffer is always aligned on 32 bit and 64 bit architectures
    uint8_t key_buffer[];
};

struct storage_info {
    size_t key_bucket_size;        // size in bytes of one whole hash table key bucket, memory aligned
    size_t hash_table_size_keys;   // size in bytes of the hole hash table keys
    size_t hash_table_size_values; // size in bytes of the hole hash table values
    uint_fast32_t max_indicies;    // number of all allocated hash table buckets
};

struct oha_lpht {
    OHA_LPHT_VALUE_BUCKET_TYPE * value_buckets;
    struct oha_lpht_key_bucket * key_buckets;
    struct oha_lpht_key_bucket * last_key_bucket;
    struct oha_lpht_key_bucket * current_bucket_to_clear;
    struct storage_info storage;
    struct oha_lpht_config config;
    uint_fast32_t elems; // current number of inserted elements
    /*
     * The maximum number of elements that could placed in the table, this value is lower than the allocated
     * number of hash table buckets, because of performance reasons. The ratio is configurable via the load factor.
     */
    uint_fast32_t max_elems;
    bool clear_mode_on;
};

__attribute__((always_inline))
static inline void * oha_lpht_get_value(const struct oha_lpht_key_bucket * const bucket)
{
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
    return bucket->value->value_buffer;
#else
    return bucket->value;
#endif
}

// does not support overflow
__attribute__((always_inline))
static inline void * oha_lpht_get_next_value(const struct oha_lpht * const table,
                                             const OHA_LPHT_VALUE_BUCKET_TYPE * const value)
{
    return oha_move_ptr_num_bytes(value, table->config.value_size + OHA_LPHT_VALUE_BUCKET_SIZE);
}

__attribute__((always_inline))
static inline uint64_t oha_lpht_hash_key(const struct oha_lpht * const table, const void * const key)
{
    return XXH3_64bits(key, table->config.key_size);
}

__attribute__((always_inline))
static inline struct oha_lpht_key_bucket * oha_lpht_get_start_bucket(const struct oha_lpht * const table, uint64_t hash)
{
    size_t index = oha_map_range_u32(hash, table->storage.max_indicies);
    return oha_move_ptr_num_bytes(table->key_buckets, table->storage.key_bucket_size * index);
}

__attribute__((always_inline))
static inline struct oha_lpht_key_bucket * oha_lpht_get_next_bucket(const struct oha_lpht * const table,
                                                                    const struct oha_lpht_key_bucket * const bucket)
{
    struct oha_lpht_key_bucket * current = oha_move_ptr_num_bytes(bucket, table->storage.key_bucket_size);
    // overflow, get to the first elem
    if (current > table->last_key_bucket) {
        current = table->key_buckets;
    }
    return current;
}

__attribute__((always_inline))
static inline void oha_lpht_swap_bucket_values(struct oha_lpht_key_bucket * const restrict a,
                                               struct oha_lpht_key_bucket * const restrict b)
{
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
    a->value->key = b;
    b->value->key = a;
#endif
    OHA_LPHT_VALUE_BUCKET_TYPE * tmp = a->value;
    a->value = b->value;
    b->value = tmp;
}

__attribute__((always_inline))
static inline bool oha_lpht_calculate_configuration(struct oha_lpht_config * const config,
                                                    struct storage_info * const values)
{
    if (config == NULL || values == NULL) {
        return false;
    }

    // TODO support zero value size as hash set
    if (config->max_elems == 0 || config->value_size == 0 || config->load_factor <= 0.0 || config->load_factor >= 1.0) {
        return false;
    }

    if (config->key_size == 0) {
        return false;
    }

    // TODO add overflow checks
    values->max_indicies = ceil((1 / config->load_factor) * config->max_elems) + 1;
    config->value_size = OHA_ALIGN_UP(config->value_size);
    values->key_bucket_size = OHA_ALIGN_UP(sizeof(struct oha_lpht_key_bucket) + config->key_size);
    values->hash_table_size_keys = values->key_bucket_size * values->max_indicies;
    values->hash_table_size_values = (config->value_size + OHA_LPHT_VALUE_BUCKET_SIZE) * values->max_indicies;

    return true;
}

__attribute__((always_inline))
static inline struct oha_lpht * oha_lpht_init_table(const struct oha_lpht_config * const config,
                                                    const struct storage_info * const storage,
                                                    struct oha_lpht * const table)
{
    table->storage = *storage;
    const struct oha_memory_fp * memory = &table->config.memory;
    table->key_buckets = oha_calloc(memory, storage->hash_table_size_keys);
    if (table->key_buckets == NULL) {
        oha_lpht_destroy(table);
        return NULL;
    }

    table->value_buckets = oha_malloc(memory, storage->hash_table_size_values);
    if (table->value_buckets == NULL) {
        oha_lpht_destroy(table);
        return NULL;
    }

    table->last_key_bucket =
        oha_move_ptr_num_bytes(table->key_buckets, table->storage.key_bucket_size * (table->storage.max_indicies - 1));
    table->max_elems = config->max_elems;
    table->current_bucket_to_clear = NULL;
    table->clear_mode_on = false;

    // connect hash buckets and value buckets
    struct oha_lpht_key_bucket * current_key_bucket = table->key_buckets;
    OHA_LPHT_VALUE_BUCKET_TYPE * current_value_bucket = table->value_buckets;
    for (size_t i = 0; i < table->storage.max_indicies; i++) {
        current_key_bucket->value = current_value_bucket;
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
        current_value_bucket->key = current_key_bucket;
#endif
        current_key_bucket = oha_lpht_get_next_bucket(table, current_key_bucket);
        current_value_bucket = oha_lpht_get_next_value(table, current_value_bucket);
    }
    return table;
}

// restores the hash table invariant
static void oha_lpht_probify(struct oha_lpht * const table,
                    struct oha_lpht_key_bucket * const start_bucket,
                    uint_fast32_t offset)
{
    struct oha_lpht_key_bucket * bucket = start_bucket;
    uint_fast32_t i = 0;
    do {
        offset++;
        i++;
        bucket = oha_lpht_get_next_bucket(table, bucket);
        if (bucket->offset >= offset || bucket->offset >= i) {
            // TODO mark bucket as dirty and do not probify
            oha_lpht_swap_bucket_values(start_bucket, bucket);
            memcpy(start_bucket->key_buffer, bucket->key_buffer, table->config.key_size);
            start_bucket->offset = bucket->offset - i;
            start_bucket->is_occupied = 1;
            bucket->is_occupied = 0;
            bucket->offset = 0;
            oha_lpht_probify(table, bucket, offset);
            return;
        }
    } while (bucket->is_occupied);
}

__attribute__((always_inline))
static inline bool oha_lpht_resize_table(struct oha_lpht * const table)
{
    if (!table->config.resizable) {
        return false;
    }

    const struct oha_memory_fp * memory = &table->config.memory;
    struct oha_lpht * tmp_table = oha_calloc(memory, sizeof(struct oha_lpht));
    if(tmp_table == NULL) {
        return false;
    }

    tmp_table->config = table->config;
    // TODO add overflow check 32 bit
    tmp_table->config.max_elems *= 2;

    struct storage_info storage;
    if (!oha_lpht_calculate_configuration(&tmp_table->config, &storage)) {
        return false;
    }

    if (oha_lpht_init_table(&tmp_table->config, &storage, tmp_table) == NULL) {
        return false;
    }

    // copy elements
    oha_lpht_clear(table);
    struct oha_key_value_pair pair;
    for (uint64_t i = 0; i < table->max_elems; i++) {
        if (!oha_lpht_get_next_element_to_remove(table, &pair)) {
            goto clean_up_and_error;
        }
        assert(pair.key != NULL);
        assert(pair.value != NULL);
        void * new_value_buffer = oha_lpht_insert(tmp_table, pair.key);
        if (new_value_buffer == NULL) {
            goto clean_up_and_error;
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

    return true;

clean_up_and_error:
    oha_lpht_destroy(tmp_table);

    return false;
}

/*
 * public functions
 */

OHA_PUBLIC_API void oha_lpht_destroy(struct oha_lpht * const table)
{
    if (table == NULL) {
        return;
    }
    const struct oha_memory_fp * memory = &table->config.memory;

    oha_free(memory, table->key_buckets);
    oha_free(memory, table->value_buckets);
    oha_free(memory, table);
}

OHA_PUBLIC_API struct oha_lpht * oha_lpht_create(const struct oha_lpht_config * const config)
{
    struct oha_lpht * const table = oha_calloc(&config->memory, sizeof(struct oha_lpht));
    if (table == NULL) {
        return NULL;
    }

    table->config = *config;
    struct storage_info storage;
    if (!oha_lpht_calculate_configuration(&table->config, &storage)) {
        oha_free(&config->memory, table);
        return NULL;
    }

    return oha_lpht_init_table(config, &storage, table);
}

// return pointer to value
OHA_PUBLIC_API void * oha_lpht_look_up(const struct oha_lpht * const table, const void * const key)
{
    if (table == NULL || key == NULL) {
        return NULL;
    }
    uint64_t hash = oha_lpht_hash_key(table, key);
    struct oha_lpht_key_bucket * bucket = oha_lpht_get_start_bucket(table, hash);
    while (bucket->is_occupied) {
        // circle + length check
        if (memcmp(bucket->key_buffer, key, table->config.key_size) == 0) {
            return oha_lpht_get_value(bucket);
        }
        bucket = oha_lpht_get_next_bucket(table, bucket);
    }
    return NULL;
}

// return pointer to value
OHA_PUBLIC_API void * oha_lpht_insert(struct oha_lpht * const table, const void * const key)
{
    if (table == NULL || key == NULL) {
        return NULL;
    }
    if (table->elems >= table->max_elems) {
        if (!oha_lpht_resize_table(table)) {
            return NULL;
        }
    }

    uint64_t hash = oha_lpht_hash_key(table, key);
    struct oha_lpht_key_bucket * bucket = oha_lpht_get_start_bucket(table, hash);

    uint_fast32_t offset = 0;
    while (bucket->is_occupied) {
        if (memcmp(bucket->key_buffer, key, table->config.key_size) == 0) {
            // already inserted
            return oha_lpht_get_value(bucket);
        }
        bucket = oha_lpht_get_next_bucket(table, bucket);
        offset++;
    }

    // insert key
    memcpy(bucket->key_buffer, key, table->config.key_size);
    bucket->offset = offset;
    bucket->is_occupied = 1;

    table->elems++;
    return oha_lpht_get_value(bucket);
}

OHA_PUBLIC_API void * oha_lpht_get_key_from_value(const void * const value)
{
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
    if (value == NULL) {
        return NULL;
    }
    OHA_LPHT_VALUE_BUCKET_TYPE * value_bucket =
        (OHA_LPHT_VALUE_BUCKET_TYPE *)((uint8_t *)value - offsetof(OHA_LPHT_VALUE_BUCKET_TYPE, value_buffer));
    assert(value_bucket->key->value == value_bucket);
    return value_bucket->key;
#else
    (void)value;
    return NULL;
#endif
}

OHA_PUBLIC_API void oha_lpht_clear(struct oha_lpht * const table)
{
    if (table == NULL) {
        return;
    }
    if (!table->clear_mode_on) {
        table->clear_mode_on = true;
        table->current_bucket_to_clear = table->key_buckets;
    }
}

OHA_PUBLIC_API bool oha_lpht_get_next_element_to_remove(struct oha_lpht * const table, struct oha_key_value_pair * const pair)
{
    if (table == NULL || !table->clear_mode_on || pair == NULL) {
        return false;
    }
    bool stop = false;

    while (table->current_bucket_to_clear <= table->last_key_bucket) {
        if (table->current_bucket_to_clear->is_occupied) {
            pair->value = oha_lpht_get_value(table->current_bucket_to_clear);
            pair->key = table->current_bucket_to_clear->key_buffer;
            stop = true;
        }
        table->current_bucket_to_clear =
            oha_move_ptr_num_bytes(table->current_bucket_to_clear, table->storage.key_bucket_size);
        if (stop == true) {
            break;
        }
    }

    return stop;
}

// return true if element was in the table
OHA_PUBLIC_API void * oha_lpht_remove(struct oha_lpht * const table, const void * const key)
{
    uint64_t hash = oha_lpht_hash_key(table, key);

    // 1. find the bucket to the given key
    struct oha_lpht_key_bucket * bucket_to_remove = NULL;
    struct oha_lpht_key_bucket * current = oha_lpht_get_start_bucket(table, hash);
    while (current->is_occupied) {
        if (memcmp(current->key_buffer, key, table->config.key_size) == 0) {
            bucket_to_remove = current;
            break;
        }
        current = oha_lpht_get_next_bucket(table, current);
    }
    // key not found
    if (bucket_to_remove == NULL) {
        return NULL;
    }

    // 2. find the last collision regarding this bucket
    struct oha_lpht_key_bucket * collision = NULL;
    uint_fast32_t start_offset = bucket_to_remove->offset;
    uint_fast32_t i = 0;
    current = oha_lpht_get_next_bucket(table, current);
    do {
        i++;
        if (current->offset == start_offset + i) {
            collision = current;
            break; // disable this to search the last collision, twice iterations vs. memcpy
        }
        current = oha_lpht_get_next_bucket(table, current);
    } while (current->is_occupied);

    void * value = oha_lpht_get_value(bucket_to_remove);
    if (collision != NULL) {
        // copy collision to the element to remove
        oha_lpht_swap_bucket_values(bucket_to_remove, collision);
        memcpy(bucket_to_remove->key_buffer, collision->key_buffer, table->config.key_size);
        collision->is_occupied = 0;
        collision->offset = 0;
        oha_lpht_probify(table, collision, 0);
    } else {
        // simple deletion
        bucket_to_remove->is_occupied = 0;
        bucket_to_remove->offset = 0;
        oha_lpht_probify(table, bucket_to_remove, 0);
    }

    table->elems--;
    return value;
}

OHA_PUBLIC_API bool oha_lpht_get_status(const struct oha_lpht * const table, struct oha_lpht_status * const status)
{
    if (table == NULL || status == NULL) {
        return false;
    }

    status->max_elems = table->max_elems;
    status->elems_in_use = table->elems;
    status->size_in_bytes =
        table->storage.hash_table_size_keys + table->storage.hash_table_size_values + sizeof(struct oha_lpht);
    return true;
}

#endif
