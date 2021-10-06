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

#define OHA_LPHT_EMPTY_BUCKET (-1)

struct oha_lpht_key_bucket {
    uint32_t index;
    uint16_t buffer_id;
    int16_t psl;            // probe sequence length
    // key buffer is always aligned on 32 bit and 64 bit architectures
    uint8_t key_buffer[];
};

struct oha_lpht {
    struct oha_lpht_key_bucket * iter; // state of the iterator
    struct oha_memory_fp memory;
    struct oha_memory_pool value_pool;
    struct oha_lpht_key_bucket * key_buckets;
    struct oha_lpht_key_bucket * last_key_bucket;
    size_t key_size;          // origin key size
    size_t key_bucket_size;   // size in bytes of one whole hash table key bucket, memory aligned
    size_t value_bucket_size; // size in bytes of one whole hash table value bucket, memory aligned
    uint32_t elems;           // current number of inserted elements
    uint32_t max_elems;       // maxium number of possible elements which can be inserted (obsolet if resizable=true)
    uint32_t max_indicies;    // number of the whole number of the underlaying array also including the left over elements

    /*
     * max_indicies = indicies_pow_of_2_minus_1 + log2_of_indicies
     */
    uint32_t indicies_pow_of_2_minus_1; // number of elements in the array which will used as fast indexing
    float max_load_factor;
    uint8_t log2_of_indicies;           // number of additional elements to avoid array bound checks
    bool resizable;
};

OHA_FORCE_INLINE void
oha_lpht_destroy_int(struct oha_lpht * const table);
OHA_FORCE_INLINE struct oha_lpht_key_bucket *
oha_lpht_insert_int(struct oha_lpht * const table, const void * const key);
OHA_FORCE_INLINE struct oha_lpht_key_bucket *
oha_lpht_look_up_int(const struct oha_lpht * const table, const void * const key);
OHA_FORCE_INLINE int
oha_lpht_iter_init_int(struct oha_lpht * const table);
OHA_FORCE_INLINE int
oha_lpht_iter_next_int(struct oha_lpht * const table, struct oha_key_value_pair * const pair);
OHA_PRIVATE_API struct oha_lpht_key_bucket *
i_oha_lpht_robin_hood_emplace(struct oha_lpht * const table,
                              void const * const key,
                              int16_t psl,
                              struct oha_lpht_key_bucket * iter);

OHA_FORCE_INLINE void
i_oha_lpht_clean_up(struct oha_lpht * const table)
{
    assert(table);
    const struct oha_memory_fp * memory = &table->memory;

    oha_free(memory, table->key_buckets);
    for (size_t i = 0; i < table->value_pool.elems; i++) {
        oha_free(memory, table->value_pool.buffers[i].data);
    }
    oha_free(memory, table->value_pool.buffers);
}

OHA_FORCE_INLINE bool
i_oha_lpht_is_occupied(const struct oha_lpht_key_bucket * const bucket)
{
    return bucket->psl >= 0;
}

OHA_FORCE_INLINE void *
i_oha_lpht_get_value(const struct oha_lpht * const table, const struct oha_lpht_key_bucket * const bucket)
{
    return oha_move_ptr_num_bytes(table->value_pool.buffers[bucket->buffer_id].data,
                                  table->value_bucket_size * bucket->index);
}

OHA_FORCE_INLINE uint32_t
i_oha_lpht_hash_key(const struct oha_lpht * const table, const void * const key)
{
    return oha_lpht_hash_32bit(key, table->key_size);
}

OHA_FORCE_INLINE struct oha_lpht_key_bucket *
i_oha_lpht_get_start_bucket(const struct oha_lpht * const table, uint32_t hash)
{
    size_t index = hash & table->indicies_pow_of_2_minus_1;
    return oha_move_ptr_num_bytes(table->key_buckets, table->key_bucket_size * index);
}

OHA_FORCE_INLINE struct oha_lpht_key_bucket *
i_oha_lpht_get_next_bucket(const struct oha_lpht * const table, const struct oha_lpht_key_bucket * const bucket)
{
#if OHA_MAX_LOG_N_PROBING
    return oha_move_ptr_num_bytes(bucket, table->key_bucket_size);
#else
    struct oha_lpht_key_bucket * current = oha_move_ptr_num_bytes(bucket, table->key_bucket_size);
    // overflow, get to the first elem
    if (current > table->last_key_bucket) {
        current = table->key_buckets;
    }
    return current;
#endif
}

OHA_FORCE_INLINE void
i_oha_lpht_calc_storage(struct oha_lpht * const table, uint32_t max_elems)
{
    assert(table);
    assert(max_elems > 0);
    assert(table->max_load_factor > 0.0);
    assert(table->max_load_factor <= 1.0);

    const uint32_t needed_elems = OHA_MAX(ceil((1.0F / table->max_load_factor) * (float)max_elems), 2);
    const uint32_t next_pow_of_2 = oha_next_power_of_two_32bit(needed_elems);
    assert(needed_elems <= next_pow_of_2);
#if OHA_MAX_LOG_N_PROBING
    table->log2_of_indicies = OHA_MAX(oha_log2_32bit(next_pow_of_2), 4);
#else
    table->log2_of_indicies = 1; // we perform the bound check, now: max_indicies = indicies_pow_of_2_minus_1 + 1
#endif
    table->indicies_pow_of_2_minus_1 = next_pow_of_2 - 1;
    table->max_indicies = table->indicies_pow_of_2_minus_1 + table->log2_of_indicies;
    if (table->resizable) {
        table->max_elems = table->max_indicies * table->max_load_factor;
    } else {
        table->max_elems = max_elems;
    }
}

OHA_FORCE_INLINE int
i_oha_lpht_init_table(struct oha_lpht * const table)
{    
    /*
     * 1. allocate needed memory
     */
    const struct oha_memory_fp * memory = &table->memory;
    table->key_buckets = oha_malloc(memory, table->key_bucket_size * (table->max_indicies));
    if (table->key_buckets == NULL) {
        i_oha_lpht_clean_up(table);
        return -1;
    }
    table->last_key_bucket =
        oha_move_ptr_num_bytes(table->key_buckets, table->key_bucket_size * (table->max_indicies - 1));
    table->iter = NULL;

    table->value_pool.buffers = oha_malloc(memory, sizeof(table->value_pool));
    if (table->value_pool.buffers == NULL) {
        i_oha_lpht_clean_up(table);
        return -2;
    }
    table->value_pool.elems = 1;

#ifdef OHA_CALLOC_LPHT_VALUE_AT_INIT
    table->value_pool.buffers[0].data = oha_calloc(memory, table->value_bucket_size * (table->max_indicies));
#else
    table->value_pool.buffers[0].data = oha_malloc(memory, table->value_bucket_size * (table->max_indicies));
#endif
    if (table->value_pool.buffers[0].data == NULL) {
        i_oha_lpht_clean_up(table);
        return -3;
    }

    /*
     * 2. connect key buckets and value buckets of both arrays
     */
    struct oha_lpht_key_bucket * iter_key = table->key_buckets;
    for (size_t i = 0; i < table->max_indicies; i++) {
        iter_key->index = i;
        iter_key->buffer_id = 0;
        iter_key->psl = OHA_LPHT_EMPTY_BUCKET;
        iter_key = oha_move_ptr_num_bytes(iter_key, table->key_bucket_size);
    }

    return 0;
}

OHA_PRIVATE_API int
i_oha_lpht_resize(struct oha_lpht * const table, const uint32_t max_elems)
{
    if (!table->resizable) {
        return -1;
    }

    if (max_elems <= table->max_elems) {
        // nothing todo
        return 0;
    }

    struct oha_lpht new_table = *table;
    i_oha_lpht_calc_storage(&new_table, max_elems);
    if (table->max_indicies == new_table.max_indicies) {
        return 0;
    }

    assert(table->max_indicies < new_table.max_indicies);
    assert(table->max_elems < new_table.max_elems);
    assert(table->max_indicies < new_table.max_indicies);

    /*
     * allocate needed memory
     */
    const struct oha_memory_fp * memory = &table->memory;
    new_table.key_buckets = oha_malloc(memory, new_table.key_bucket_size * new_table.max_indicies);
    if (new_table.key_buckets == NULL) {
        return -2;
    }
    new_table.last_key_bucket =
        oha_move_ptr_num_bytes(new_table.key_buckets, new_table.key_bucket_size * (new_table.max_indicies - 1));
    new_table.iter = NULL;

    // TODO reduce memory overhead of allocation
    const uint32_t new_needed_elems = new_table.max_indicies - table->elems;
    void * new_data =
#ifdef OHA_CALLOC_LPHT_VALUE_AT_INIT
        oha_calloc(memory, new_table.value_bucket_size * new_needed_elems);
#else
        oha_malloc(memory, new_table.value_bucket_size * new_needed_elems);
#endif
    if (new_data == NULL) {
        oha_free(memory, new_table.key_buckets);
        return -3;
    }

    size_t num_buffers = new_table.value_pool.elems;
    const size_t new_buffer_id = num_buffers;
    if (!oha_add_entry_to_array(
            memory, (void *)&new_table.value_pool.buffers, sizeof(*new_table.value_pool.buffers), &num_buffers)) {
        oha_free(memory, new_table.key_buckets);
        oha_free(memory, new_data);
        return -4;
    }
    assert(num_buffers == ((size_t)new_table.value_pool.elems) + 1);

    // update table
    new_table.value_pool.buffers[new_table.value_pool.elems].data = new_data;
    new_table.value_pool.elems = num_buffers;
    new_table.max_elems = max_elems;
    new_table.elems = 0;

    // mark new table key buckets as empty
    for (struct oha_lpht_key_bucket * iter = new_table.key_buckets; iter <= new_table.last_key_bucket;
         iter = oha_move_ptr_num_bytes(iter, new_table.key_bucket_size)) {
        iter->psl = OHA_LPHT_EMPTY_BUCKET;
    }

    // rehash and emplace all old keys
    uint32_t new_free_value_id = table->elems;
    for (struct oha_lpht_key_bucket * iter = table->key_buckets; iter <= table->last_key_bucket;
         iter = oha_move_ptr_num_bytes(iter, table->key_bucket_size)) {
        if (!i_oha_lpht_is_occupied(iter)) {
            iter->index = new_free_value_id;
            iter->buffer_id = new_buffer_id;
            new_free_value_id++;
            continue;
        }
        assert(iter->index < table->max_indicies);
        uint32_t hash = i_oha_lpht_hash_key(&new_table, iter->key_buffer);
        struct oha_lpht_key_bucket * new_bucket = i_oha_lpht_get_start_bucket(&new_table, hash);
        struct oha_lpht_key_bucket * new_place =
            i_oha_lpht_robin_hood_emplace(&new_table, iter->key_buffer, 0, new_bucket);

        assert(new_place);
        assert(oha_lpht_look_up_int(&new_table, iter->key_buffer) == new_place);
        new_place->index = iter->index;
        new_place->buffer_id = iter->buffer_id;
    }
    assert(table->elems == new_table.elems); // copied all inserted elemets to new structure

    uint32_t tmp_bucket_number = table->elems;
    for (struct oha_lpht_key_bucket * iter = new_table.key_buckets; iter <= new_table.last_key_bucket;
         iter = oha_move_ptr_num_bytes(iter, new_table.key_bucket_size)) {
        if (iter->psl == OHA_LPHT_EMPTY_BUCKET) {
            iter->index = tmp_bucket_number;
            iter->buffer_id = new_buffer_id;
            tmp_bucket_number++;
        }
    }
    assert(tmp_bucket_number == new_table.max_indicies);

    oha_free(memory, table->key_buckets);
    *table = new_table;

    return 0;
}

OHA_FORCE_INLINE int
i_oha_lpht_grow(struct oha_lpht * const table)
{
    return i_oha_lpht_resize(table, 2 * table->max_elems);
}

OHA_PRIVATE_API struct oha_lpht_key_bucket *
i_oha_lpht_robin_hood_emplace(struct oha_lpht * const table,
                              void const * const key,
                              int16_t psl,
                              struct oha_lpht_key_bucket * iter)
{
    if (table->elems >= table->max_elems) {
        // double size table
        if (i_oha_lpht_grow(table)) {
            return NULL;
        }
        return oha_lpht_insert_int(table, key);
    } else if (!i_oha_lpht_is_occupied(iter)) {
        // terminate robin hood insertion, we found a empty bucket
        memcpy(iter->key_buffer, key, table->key_size);
        iter->psl = psl;
        table->elems++;
        return iter;
    }

    // 1. copy poor bucket to temporal memory buffer
    char buffer[table->key_bucket_size];
    struct oha_lpht_key_bucket * tmp_key_bucket = (struct oha_lpht_key_bucket *)buffer;
    memcpy(tmp_key_bucket->key_buffer, key, table->key_size);
    tmp_key_bucket->index = iter->index;

    // swap poor and the rich bucket
    struct oha_lpht_key_bucket * const inserted_key_bucket = iter;
    i_oha_swap_memory(iter->key_buffer, tmp_key_bucket->key_buffer, table->key_size);
    OHA_SWAP(iter->psl, psl);

    for (++psl, iter = i_oha_lpht_get_next_bucket(table, iter);;
         ++psl, iter = i_oha_lpht_get_next_bucket(table, iter)) {
        if (!i_oha_lpht_is_occupied(iter)) {
            // terminate robin hood insertion, we found a empty bucket
            iter->psl = psl;
            memcpy(iter->key_buffer, tmp_key_bucket->key_buffer, table->key_size);
            OHA_SWAP(tmp_key_bucket->index, iter->index);
            table->elems++;
            inserted_key_bucket->index = tmp_key_bucket->index;
            return inserted_key_bucket;
        } else if (psl > iter->psl) {
            // apply robin hood creed and swap the poor and the rich bucket
            i_oha_swap_memory(iter->key_buffer, tmp_key_bucket->key_buffer, table->key_size);
            OHA_SWAP(iter->psl, psl);
            OHA_SWAP(tmp_key_bucket->index, iter->index);
        }
#if OHA_MAX_LOG_N_PROBING
        else if (psl > table->log2_of_indicies) {
            // we need to resize the table, otherwise
            // the key can be placed in not allocated memory
            if (i_oha_lpht_grow(table) != 0) {
                return NULL;
            }
            // try again
            return oha_lpht_insert_int(table, key);
        }
#endif
    }
}

OHA_FORCE_INLINE void
oha_lpht_destroy_int(struct oha_lpht * const table)
{
    assert(table);
    const struct oha_memory_fp * memory = &table->memory;

    i_oha_lpht_clean_up(table);
    oha_free(memory, table);
}

OHA_FORCE_INLINE struct oha_lpht *
oha_lpht_create_int(const struct oha_lpht_config * const config)
{
    assert(config);
    // TODO support zero value size as hash set
    if (config->max_elems == 0 || config->max_load_factor <= 0.0 ||
        config->max_load_factor >= 1.0) {
        return NULL;
    }

    if (config->key_size < 4) {
        return NULL;
    }

    struct oha_lpht * const table = oha_calloc(&config->memory, sizeof(struct oha_lpht));
    if (table == NULL) {
        return NULL;
    }

    // copy config
    table->key_size = config->key_size;
    table->key_bucket_size = OHA_ALIGN_UP(sizeof(struct oha_lpht_key_bucket) + config->key_size);
    table->value_bucket_size = OHA_ALIGN_UP(config->value_size);
    table->max_load_factor = config->max_load_factor;
    table->memory = config->memory;
    table->resizable = config->resizable;
    table->max_load_factor = OHA_MAX(0.5, config->max_load_factor);
    i_oha_lpht_calc_storage(table, config->max_elems);

    if (0 != i_oha_lpht_init_table(table)) {
        oha_free(&config->memory, table);
        return NULL;
    }
    return table;
}

// return pointer to value
OHA_FORCE_INLINE struct oha_lpht_key_bucket *
oha_lpht_look_up_int(const struct oha_lpht * const table, const void * const key)
{
    assert(table);
    assert(key);
    uint32_t hash = i_oha_lpht_hash_key(table, key);
    // TODO add check for max prob counter

    struct oha_lpht_key_bucket * iter = i_oha_lpht_get_start_bucket(table, hash);
    for (int32_t psl = 0; psl <= iter->psl; iter = i_oha_lpht_get_next_bucket(table, iter), ++psl) {
        // circle + length check
        if (memcmp(iter->key_buffer, key, table->key_size) == 0) {
            return iter;
        }
    }
    return NULL;
}

// return pointer to value
OHA_PRIVATE_API struct oha_lpht_key_bucket *
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
        if (memcmp(iter->key_buffer, key, table->key_size) == 0) {
            // already inserted
            return iter;
        }
    }

    // unfair, we need to apply the robin hood creed
    // the new key was definite not in the table, otherwise we already found it, because of
    // the robin hood invariant
    return i_oha_lpht_robin_hood_emplace(table, key, psl, iter);
}

OHA_FORCE_INLINE int
oha_lpht_iter_init_int(struct oha_lpht * const table)
{
    assert(table);

    table->iter = table->key_buckets;
    return 0;
}

OHA_FORCE_INLINE int
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
OHA_FORCE_INLINE void *
oha_lpht_remove_int(struct oha_lpht * const table, const void * const key)
{
    assert(table && key);
    struct oha_lpht_key_bucket * bucket_to_remove = oha_lpht_look_up_int(table, key);
    if (bucket_to_remove == NULL) {
        return NULL;
    }

    const uint32_t value_bucket_index = bucket_to_remove->index;
    const uint16_t buffer_id = bucket_to_remove->buffer_id;

    // remove bucket
    bucket_to_remove->psl = OHA_LPHT_EMPTY_BUCKET;

    struct oha_lpht_key_bucket * iter = bucket_to_remove;
    struct oha_lpht_key_bucket * iter_next = i_oha_lpht_get_next_bucket(table, iter);
    while (iter_next->psl > 0) {

        // back shift and decrement psl
        memcpy(iter->key_buffer, iter_next->key_buffer, table->key_size);
        OHA_SWAP(iter->index, iter_next->index);
        iter->psl = iter_next->psl - 1;
        iter_next->psl = OHA_LPHT_EMPTY_BUCKET;

        iter = iter_next;
        iter_next = i_oha_lpht_get_next_bucket(table, iter);
    };

    table->elems--;

    return oha_move_ptr_num_bytes(table->value_pool.buffers[buffer_id].data, table->value_bucket_size * value_bucket_index);
}

OHA_FORCE_INLINE int
oha_lpht_get_status_int(const struct oha_lpht * const table, struct oha_lpht_status * const status)
{
    assert(table && status);

    status->max_elems = table->max_elems;
    status->elems_in_use = table->elems;
    status->size_in_bytes =
        // key buckets
        table->key_bucket_size * (table->max_indicies) +
        // value buckets
        table->value_bucket_size * (table->max_indicies) +
        // table offset size
        sizeof(struct oha_lpht);
    status->current_load_factor = (float)table->elems / (float)(table->max_indicies);
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
    struct oha_lpht_key_bucket * bucket = oha_lpht_insert_int(table, key);
    if (bucket != NULL) {
        return i_oha_lpht_get_value(table, bucket);
    }
    return NULL;
}

OHA_PUBLIC_API int
oha_lpht_reserve(struct oha_lpht * const table, uint32_t elements)
{
#if OHA_NULL_POINTER_CHECKS
    if (table == NULL) {
        return -1;
    }
#endif
    return i_oha_lpht_resize(table, elements);
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
