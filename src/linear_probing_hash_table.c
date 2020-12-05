#include "oha.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <xxhash.h>

#include "utils.h"

#define XXHASH_SEED 0xc800c831bc63dff8

#ifdef OHA_FIX_KEY_SIZE_IN_BYTES
#if OHA_FIX_KEY_SIZE_IN_BYTES == 0
#error "unsupported compile time key size"
#endif
// let the the compiler optimize memory calls by compile time constant
#define MEMCPY_KEY(dest, src, n) memcpy(dest, src, OHA_FIX_KEY_SIZE_IN_BYTES);
#define MEMCMP_KEY(a, b, n) memcmp(a, b, OHA_FIX_KEY_SIZE_IN_BYTES)
#else
#define MEMCPY_KEY(dest, src, n) memcpy(dest, src, n);
#define MEMCMP_KEY(a, b, n) memcmp(a, b, n)
#endif

#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
struct key_bucket;
struct value_bucket {
    struct key_bucket * key;
    uint8_t value_buffer[];
};
#define VALUE_BUCKET_TYPE struct value_bucket
#define TABLE_VALUE_BUCKET_SIZE sizeof(struct value_bucket)
#else
#define VALUE_BUCKET_TYPE void
#define TABLE_VALUE_BUCKET_SIZE 0
#endif

struct key_bucket {
    VALUE_BUCKET_TYPE * value;
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
    VALUE_BUCKET_TYPE * value_buckets;
    struct key_bucket * key_buckets;
    struct key_bucket * last_key_bucket;
    struct key_bucket * current_bucket_to_clear;
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

static inline void * get_value(struct key_bucket * bucket)
{
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
    return bucket->value->value_buffer;
#else
    return bucket->value;
#endif
}

// does not support overflow
static void * get_next_value(struct oha_lpht * table, VALUE_BUCKET_TYPE * value)
{
    return move_ptr_num_bytes(value, table->config.value_size);
}

static uint64_t hash_key(struct oha_lpht * table, const void * key)
{
#ifdef OHA_FIX_KEY_SIZE_IN_BYTES
    (void)table;
    return XXH64(key, OHA_FIX_KEY_SIZE_IN_BYTES, XXHASH_SEED);
#else
    return XXH64(key, table->config.key_size, XXHASH_SEED);
#endif
}

static struct key_bucket * get_start_bucket(struct oha_lpht * table, uint64_t hash)
{
    // TODO use shift if max_indicies is pow of 2
    size_t index = hash % table->storage.max_indicies;
    return move_ptr_num_bytes(table->key_buckets, table->storage.key_bucket_size * index);
}

static struct key_bucket * get_next_bucket(struct oha_lpht * table, struct key_bucket * bucket)
{
    struct key_bucket * current = move_ptr_num_bytes(bucket, table->storage.key_bucket_size);
    // overflow, get to the first elem
    if (current > table->last_key_bucket) {
        current = table->key_buckets;
    }
    return current;
}

static void swap_bucket_values(struct key_bucket * restrict a, struct key_bucket * restrict b)
{
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
    a->value->key = b;
    b->value->key = a;
#endif
    VALUE_BUCKET_TYPE * tmp = a->value;
    a->value = b->value;
    b->value = tmp;
}

static bool calculate_storage_values(struct oha_lpht_config * config, struct storage_info * values)
{
    if (config == NULL || values == NULL) {
        return false;
    }

    // TODO support zero value size as hash set
    if (config->max_elems == 0 || config->value_size == 0 || config->load_factor <= 0.0 || config->load_factor >= 1.0) {
        return false;
    }

#ifndef OHA_FIX_KEY_SIZE_IN_BYTES
    if (config->key_size == 0) {
        return false;
    }
#else
    config->key_size = OHA_FIX_KEY_SIZE_IN_BYTES;
#endif

    // TODO add overflow checks
    values->max_indicies = ceil((1 / config->load_factor) * config->max_elems) + 1;
    config->value_size = TABLE_VALUE_BUCKET_SIZE + add_alignment(config->value_size);
    values->key_bucket_size = add_alignment(sizeof(struct key_bucket) + config->key_size);
    values->hash_table_size_keys = values->key_bucket_size * values->max_indicies;
    values->hash_table_size_values = config->value_size * values->max_indicies;

    return true;
}

static struct oha_lpht * init_table_value(const struct oha_lpht_config * config,
                                          const struct storage_info * storage,
                                          struct oha_lpht * table)
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
        move_ptr_num_bytes(table->key_buckets, table->storage.key_bucket_size * (table->storage.max_indicies - 1));
    table->max_elems = config->max_elems;
    table->current_bucket_to_clear = NULL;
    table->clear_mode_on = false;

    // connect hash buckets and value buckets
    struct key_bucket * current_key_bucket = table->key_buckets;
    VALUE_BUCKET_TYPE * current_value_bucket = table->value_buckets;
    for (size_t i = 0; i < table->storage.max_indicies; i++) {
        current_key_bucket->value = current_value_bucket;
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
        current_value_bucket->key = current_key_bucket;
#endif
        current_key_bucket = get_next_bucket(table, current_key_bucket);
        current_value_bucket = get_next_value(table, current_value_bucket);
    }
    return table;
}

// restores the hash table invariant
static void probify(struct oha_lpht * table, struct key_bucket * start_bucket, uint_fast32_t offset)
{
    struct key_bucket * bucket = start_bucket;
    uint_fast32_t i = 0;
    do {
        offset++;
        i++;
        bucket = get_next_bucket(table, bucket);
        if (bucket->offset >= offset || bucket->offset >= i) {
            // TODO mark bucket as dirty and do not probify
            swap_bucket_values(start_bucket, bucket);
            MEMCPY_KEY(start_bucket->key_buffer, bucket->key_buffer, table->config.key_size);
            start_bucket->offset = bucket->offset - i;
            start_bucket->is_occupied = 1;
            bucket->is_occupied = 0;
            bucket->offset = 0;
            probify(table, bucket, offset);
            return;
        }
    } while (bucket->is_occupied);
}

static bool resize_table(struct oha_lpht * table)
{
    if (!table->config.resizable) {
        return false;
    }

    struct oha_lpht tmp_table = {0};
    tmp_table.config = table->config;
    tmp_table.config.max_elems *= 2;

    struct storage_info storage;
    if (!calculate_storage_values(&tmp_table.config, &storage)) {
        return false;
    }

    if (init_table_value(&tmp_table.config, &storage, &tmp_table) == NULL) {
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
        void * new_value_buffer = oha_lpht_insert(&tmp_table, pair.key);
        if (new_value_buffer == NULL) {
            goto clean_up_and_error;
        }

        // TODO: maybe do not memcpy, let old buffer alive? -> add free value list
        memcpy(new_value_buffer, pair.value, tmp_table.config.value_size);
    }

    // destroy old table buffers
    oha_free(&table->config.memory, table->key_buckets);
    oha_free(&table->config.memory, table->value_buckets);

    // copy new table
    *table = tmp_table;

    return true;

clean_up_and_error:
    oha_free(&tmp_table.config.memory, tmp_table.key_buckets);
    oha_free(&tmp_table.config.memory, tmp_table.value_buckets);
    return false;
}

/*
 * public functions
 */

void oha_lpht_destroy(struct oha_lpht * table)
{
    if (table == NULL) {
        return;
    }
    const struct oha_memory_fp * memory = &table->config.memory;

    oha_free(memory, table->key_buckets);
    oha_free(memory, table->value_buckets);
    oha_free(memory, table);
}

struct oha_lpht * oha_lpht_create(const struct oha_lpht_config * config)
{
    struct oha_lpht * table = oha_calloc(&config->memory, sizeof(struct oha_lpht));
    if (table == NULL) {
        return NULL;
    }

    table->config = *config;
    struct storage_info storage;
    if (!calculate_storage_values(&table->config, &storage)) {
        oha_free(&config->memory, table);
        return NULL;
    }

    return init_table_value(config, &storage, table);
}

// return pointer to value
void * oha_lpht_look_up(struct oha_lpht * table, const void * key)
{
    if (table == NULL || key == NULL) {
        return NULL;
    }
    uint64_t hash = hash_key(table, key);
    struct key_bucket * bucket = get_start_bucket(table, hash);
    while (bucket->is_occupied) {
        // circle + length check
        if (MEMCMP_KEY(bucket->key_buffer, key, table->config.key_size) == 0) {
            return get_value(bucket);
        }
        bucket = get_next_bucket(table, bucket);
    }
    return NULL;
}

// return pointer to value
void * oha_lpht_insert(struct oha_lpht * table, const void * key)
{
    if (table == NULL || key == NULL) {
        return NULL;
    }
    if (table->elems >= table->max_elems) {
        if (!resize_table(table)) {
            return NULL;
        }
    }

    uint64_t hash = hash_key(table, key);
    struct key_bucket * bucket = get_start_bucket(table, hash);

    uint_fast32_t offset = 0;
    while (bucket->is_occupied) {
        if (MEMCMP_KEY(bucket->key_buffer, key, table->config.key_size) == 0) {
            // already inserted
            return get_value(bucket);
        }
        bucket = get_next_bucket(table, bucket);
        offset++;
    }

    // insert key
    MEMCPY_KEY(bucket->key_buffer, key, table->config.key_size);
    bucket->offset = offset;
    bucket->is_occupied = 1;

    table->elems++;
    return get_value(bucket);
}

void * oha_lpht_get_key_from_value(const void * value)
{
#ifdef OHA_WITH_KEY_FROM_VALUE_SUPPORT
    if (value == NULL) {
        return NULL;
    }
    struct value_bucket * value_bucket =
        (struct value_bucket *)((uint8_t *)value - offsetof(struct value_bucket, value_buffer));
    assert(value_bucket->key->value == value_bucket);
    return value_bucket->key;
#else
    (void)value;
    return NULL;
#endif
}

void oha_lpht_clear(struct oha_lpht * table)
{
    if (table == NULL) {
        return;
    }
    if (!table->clear_mode_on) {
        table->clear_mode_on = true;
        table->current_bucket_to_clear = table->key_buckets;
    }
}

bool oha_lpht_get_next_element_to_remove(struct oha_lpht * table, struct oha_key_value_pair * pair)
{
    if (table == NULL || !table->clear_mode_on || pair == NULL) {
        return false;
    }
    bool stop = false;

    while (table->current_bucket_to_clear <= table->last_key_bucket) {
        if (table->current_bucket_to_clear->is_occupied) {
            pair->value = get_value(table->current_bucket_to_clear);
            pair->key = table->current_bucket_to_clear->key_buffer;
            stop = true;
        }
        table->current_bucket_to_clear =
            move_ptr_num_bytes(table->current_bucket_to_clear, table->storage.key_bucket_size);
        if (stop == true) {
            break;
        }
    }

    return stop;
}

// return true if element was in the table
void * oha_lpht_remove(struct oha_lpht * table, const void * key)
{
    uint64_t hash = hash_key(table, key);

    // 1. find the bucket to the given key
    struct key_bucket * bucket_to_remove = NULL;
    struct key_bucket * current = get_start_bucket(table, hash);
    while (current->is_occupied) {
        if (MEMCMP_KEY(current->key_buffer, key, table->config.key_size) == 0) {
            bucket_to_remove = current;
            break;
        }
        current = get_next_bucket(table, current);
    }
    // key not found
    if (bucket_to_remove == NULL) {
        return NULL;
    }

    // 2. find the last collision regarding this bucket
    struct key_bucket * collision = NULL;
    uint_fast32_t start_offset = bucket_to_remove->offset;
    uint_fast32_t i = 0;
    current = get_next_bucket(table, current);
    do {
        i++;
        if (current->offset == start_offset + i) {
            collision = current;
            break; // disable this to search the last collision, twice iterations vs. memcpy
        }
        current = get_next_bucket(table, current);
    } while (current->is_occupied);

    void * value = get_value(bucket_to_remove);
    if (collision != NULL) {
        // copy collision to the element to remove
        swap_bucket_values(bucket_to_remove, collision);
        MEMCPY_KEY(bucket_to_remove->key_buffer, collision->key_buffer, table->config.key_size);
        collision->is_occupied = 0;
        collision->offset = 0;
        probify(table, collision, 0);
    } else {
        // simple deletion
        bucket_to_remove->is_occupied = 0;
        bucket_to_remove->offset = 0;
        probify(table, bucket_to_remove, 0);
    }

    table->elems--;
    return value;
}

bool oha_lpht_get_status(struct oha_lpht * table, struct oha_lpht_status * status)
{
    if (table == NULL || status == NULL) {
        return false;
    }

    status->max_elems = table->max_elems;
    status->elems_in_use = table->elems;
    status->size_in_bytes = table->config.value_size;
    return true;
}
