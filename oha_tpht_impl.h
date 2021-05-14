#ifndef OHA_TEMPORAL_PRIORITIZED_HASH_TABLE_H_
#define OHA_TEMPORAL_PRIORITIZED_HASH_TABLE_H_

#include "oha.h"

#include <stdlib.h>
#include <limits.h>

#include "oha_utils.h"

struct oha_tpht_storage_info {
    size_t hash_table_size;
    uint8_t number_of_timeout_slots;
};

struct oha_tpht_timeout_slot {
    struct oha_bh * bh;
    int64_t timeout;
};

struct oha_tpht {
    struct oha_lpht * lpht;
    struct oha_tpht_timeout_slot * slots;
    size_t num_timeout_slots;
    int64_t last_timestamp;
    struct oha_tpht_storage_info storage;
    struct oha_lpht_config lpht_config;
};

struct oha_tpht_value_bucket {
    void * heap_value;
    union {
        uint8_t slot_id;
        void * memory_alignment;
    };
    uint8_t data[];
};

OHA_FORCE_INLINE void
oha_tpht_destroy_int(struct oha_tpht * const tpht);

OHA_FORCE_INLINE void
i_oha_tpht_connect_heap_with_hash_table(struct oha_tpht_value_bucket * hash_table_value,
                                        void * heap_value,
                                        const void * const origin_key,
                                        size_t key_size,
                                        uint8_t slot_id)
{
    // TODO encode slot id in pointer
    memcpy(heap_value, origin_key, key_size);
    hash_table_value->heap_value = heap_value;
    hash_table_value->slot_id = slot_id;
}

/*
 * public functions
 */

OHA_FORCE_INLINE struct oha_tpht *
oha_tpht_create_int(const struct oha_tpht_config * const config)
{
    assert(config);

    struct oha_tpht * table = oha_calloc(&config->lpht_config.memory, sizeof(struct oha_tpht));
    if (table == NULL) {
        return NULL;
    }

    table->lpht_config = config->lpht_config;
    table->lpht_config.value_size += sizeof(struct oha_tpht_value_bucket); // space for the connection to the heap
    table->lpht = oha_lpht_create_int(&table->lpht_config);
    if (table->lpht == NULL) {
        oha_tpht_destroy_int(table);
        return NULL;
    }

    return table;
}

OHA_FORCE_INLINE int8_t
oha_tpht_add_timeout_slot_int(struct oha_tpht * const tpht, int64_t timeout, uint32_t num_elements, bool resizable)
{
    assert(tpht);

    if (tpht->num_timeout_slots >= SCHAR_MAX) {
        return -2;
    }

    const size_t next_free_index = tpht->num_timeout_slots;

    if (!oha_add_entry_to_array(
            &tpht->lpht_config.memory, (void *)&tpht->slots, sizeof(*tpht->slots), &tpht->num_timeout_slots)) {
        return -3;
    }

    const struct oha_bh_config bh_config = {
        .memory = tpht->lpht_config.memory,
        .value_size = tpht->lpht_config.key_size,
        .max_elems = num_elements,
        .resizable = resizable,
    };

    struct oha_tpht_timeout_slot * next_free_slot = &tpht->slots[next_free_index];

    next_free_slot->bh = oha_bh_create_int(&bh_config);
    next_free_slot->timeout = timeout;
    if (next_free_slot->bh == NULL) {
        oha_remove_entry_from_array(&tpht->lpht_config.memory,
                                    (void *)&tpht->slots,
                                    sizeof(*tpht->slots),
                                    &tpht->num_timeout_slots,
                                    next_free_index);
        return -4;
    }

    return tpht->num_timeout_slots;
}

OHA_FORCE_INLINE void
oha_tpht_destroy_int(struct oha_tpht * const tpht)
{
    assert(tpht);

    oha_lpht_destroy(tpht->lpht);
    tpht->lpht = NULL;

    for (size_t i = 0; i < tpht->num_timeout_slots; i++) {
        oha_bh_destroy(tpht->slots[i].bh);
    }
    oha_free(&tpht->lpht_config.memory, tpht->slots);
    tpht->slots = NULL;

    oha_free(&tpht->lpht_config.memory, tpht);
}

OHA_FORCE_INLINE int
oha_tpht_increase_global_time_int(struct oha_tpht * const tpht, int64_t timestamp)
{
    assert(tpht);
    if (timestamp < tpht->last_timestamp) {
        return 1;
    }
    tpht->last_timestamp = timestamp;
    return 0;
}

OHA_FORCE_INLINE void *
oha_tpht_insert_int(struct oha_tpht * const tpht, const void * const key, uint8_t timeout_slot_id)
{
    assert(tpht);
    assert(key);

    if (timeout_slot_id > tpht->num_timeout_slots) {
        return NULL;
    }

    struct oha_tpht_value_bucket * hash_table_value = oha_lpht_insert_int(tpht->lpht, key);
    if (hash_table_value == NULL) {
        return NULL;
    }

    if (timeout_slot_id > 0) {
        void * heap_value = oha_bh_insert_int(tpht->slots[tpht->num_timeout_slots - 1].bh, tpht->last_timestamp);
        if (heap_value == NULL) {
            oha_lpht_remove_int(tpht->lpht, key);
            return NULL;
        }

        i_oha_tpht_connect_heap_with_hash_table(
            hash_table_value, heap_value, key, tpht->lpht_config.key_size, timeout_slot_id);
    } else {
        hash_table_value->slot_id = 0;
    }

    return hash_table_value->data;
}

OHA_FORCE_INLINE void *
oha_tpht_look_up_int(const struct oha_tpht * const tpht, const void * const key)
{
    assert(tpht);
    assert(key);

    struct oha_tpht_value_bucket * value = NULL;
    struct oha_lpht_key_bucket * bucket = oha_lpht_look_up_int(tpht->lpht, key);
    if (bucket != NULL) {
        value = i_oha_lpht_get_value(tpht->lpht, bucket);
    } else {
        return NULL;
    }

    return value->data;
}

OHA_FORCE_INLINE void *
oha_tpht_remove_int(struct oha_tpht * const tpht, const void * const key)
{

    assert(tpht);
    assert(key);

    struct oha_tpht_value_bucket * value = oha_lpht_remove_int(tpht->lpht, key);
    if (value == NULL) {
        return NULL;
    }

    if (value->slot_id > 0) {
        const uint8_t slot = value->slot_id - 1;
        // remove also from heap
        assert(value->heap_value);
        assert(tpht->slots[slot].bh);
        if (OHA_BH_NOT_FOUND != oha_bh_change_key_int(tpht->slots[slot].bh, value->heap_value, OHA_BH_NOT_FOUND)) {
            assert(0 && "unexpected missing heap element. bug in  'oha_tpht_insert' function.");
            return value->data;
        }
        oha_bh_delete_min_int(tpht->slots[slot].bh);
    }

    return value->data;
}

OHA_FORCE_INLINE size_t
oha_tpht_next_timeout_entries_int(struct oha_tpht * const tpht,
                                  struct oha_key_value_pair * const next_pair,
                                  size_t num_pairs)
{
    assert(tpht);
    assert(next_pair);

    size_t num_found_entries = 0;
    for (size_t slot = 0; slot < tpht->num_timeout_slots; slot++) {
        do {
            if (num_pairs == num_found_entries) {
                return num_found_entries;
            }
            struct oha_tpht_timeout_slot * current_slot = &tpht->slots[slot];
            const int64_t min_ts = oha_bh_find_min_int(current_slot->bh);
            if (OHA_BH_NOT_FOUND == min_ts) {
                break;
            }

            if (tpht->last_timestamp < min_ts + current_slot->timeout) {
                break;
            } else {
                next_pair[num_found_entries].key = oha_bh_delete_min_int(current_slot->bh);
                assert(next_pair[num_found_entries].key != NULL);
                struct oha_tpht_value_bucket * value =
                    oha_lpht_remove_int(tpht->lpht, next_pair[num_found_entries].key);
                assert(value != NULL);
                next_pair[num_found_entries].value = value->data;
                num_found_entries++;
                assert(value->heap_value != NULL);
            }

        } while (1);
    }

    return num_found_entries;
}

OHA_FORCE_INLINE void *
oha_tpht_update_time_for_entry_int(struct oha_tpht * const tpht, const void * const key, int64_t new_timestamp)
{

    assert(tpht);
    assert(key);

    if (new_timestamp < tpht->last_timestamp) {
        return NULL;
    }

    struct oha_tpht_value_bucket * value = NULL;
    struct oha_lpht_key_bucket * bucket = oha_lpht_look_up_int(tpht->lpht, key);
    if (bucket != NULL) {
        value = i_oha_lpht_get_value(tpht->lpht, bucket);
    } else {
        return NULL;
    }

    if (value->slot_id > 0 && value->slot_id <= tpht->num_timeout_slots) {
        const uint8_t slot = value->slot_id - 1;
        int64_t updated_timestamp = oha_bh_change_key_int(tpht->slots[slot].bh, value->heap_value, new_timestamp);
        if (new_timestamp != updated_timestamp) {
            return NULL;
        }
        return value->data;
    }

    return NULL;
}

OHA_FORCE_INLINE void *
oha_tpht_set_timeout_slot_int(struct oha_tpht * tpht, const void * key, uint8_t new_slot)
{
    assert(tpht);
    assert(key);

    if (new_slot == 0 || new_slot > tpht->num_timeout_slots) {
        return NULL;
    }

    struct oha_tpht_value_bucket * hash_table_value = NULL;
    struct oha_lpht_key_bucket * bucket = oha_lpht_look_up_int(tpht->lpht, key);
    if (bucket != NULL) {
        hash_table_value = i_oha_lpht_get_value(tpht->lpht, bucket);
    } else {
        return NULL;
    }

    if (hash_table_value->slot_id == new_slot) {
        return hash_table_value->data;
    }

    int64_t timestamp;
    if (hash_table_value->slot_id > 0) {
        // add check if new slot has at least space for one new element
        uint8_t old_slot = hash_table_value->slot_id - 1;
        void * old_heap_value = oha_bh_remove_int(tpht->slots[old_slot].bh, hash_table_value->heap_value, &timestamp);
        assert(old_heap_value != NULL);
        assert(0 == memcmp(old_heap_value, key, tpht->lpht_config.key_size));
        (void)old_heap_value;
    } else {
        timestamp = tpht->last_timestamp;
    }

    uint8_t slot = new_slot - 1;
    void * new_heap_value = oha_bh_insert_int(tpht->slots[slot].bh, timestamp);
    if (new_heap_value == NULL) {
        return NULL;
    }

    i_oha_tpht_connect_heap_with_hash_table(
        hash_table_value, new_heap_value, key, tpht->lpht_config.key_size, new_slot);

    return hash_table_value->data;
}

/**********************************************************************************************************************
 *
 * public interface functions section
 *
 *********************************************************************************************************************/

OHA_PUBLIC_API struct oha_tpht *
oha_tpht_create(const struct oha_tpht_config * const config)
{
#if OHA_NULL_POINTER_CHECKS
    if (config == NULL) {
        return NULL;
    }
#endif
    return oha_tpht_create_int(config);
}

OHA_PUBLIC_API int8_t
oha_tpht_add_timeout_slot(struct oha_tpht * const tpht, int64_t timeout, uint32_t num_elements, bool resizable)
{
#if OHA_NULL_POINTER_CHECKS
    if (tpht == NULL) {
        return -1;
    }
#endif
    return oha_tpht_add_timeout_slot_int(tpht, timeout, num_elements, resizable);
}

OHA_PUBLIC_API void
oha_tpht_destroy(struct oha_tpht * const tpht)
{
#if OHA_NULL_POINTER_CHECKS
    if (tpht == NULL) {
        return;
    }
#endif
    oha_tpht_destroy_int(tpht);
}

OHA_PUBLIC_API int
oha_tpht_increase_global_time(struct oha_tpht * const tpht, int64_t timestamp)
{
#if OHA_NULL_POINTER_CHECKS
    if (tpht == NULL) {
        return -1;
    }
#endif
    return oha_tpht_increase_global_time_int(tpht, timestamp);
}

OHA_PUBLIC_API void *
oha_tpht_insert(struct oha_tpht * const tpht, const void * const key, uint8_t timeout_slot_id)
{
#if OHA_NULL_POINTER_CHECKS
    if (tpht == NULL || key == NULL) {
        return NULL;
    }
#endif
    return oha_tpht_insert_int(tpht, key, timeout_slot_id);
}

OHA_PUBLIC_API void *
oha_tpht_look_up(const struct oha_tpht * const tpht, const void * const key)
{
#if OHA_NULL_POINTER_CHECKS
    if (tpht == NULL || key == NULL) {
        return NULL;
    }
#endif
    return oha_tpht_look_up_int(tpht, key);
}

OHA_PUBLIC_API void *
oha_tpht_remove(struct oha_tpht * const tpht, const void * const key)
{
#if OHA_NULL_POINTER_CHECKS
    if (tpht == NULL || key == NULL) {
        return NULL;
    }
#endif
    return oha_tpht_remove_int(tpht, key);
}

OHA_PUBLIC_API size_t
oha_tpht_next_timeout_entries(struct oha_tpht * const tpht,
                              struct oha_key_value_pair * const next_pair,
                              size_t num_pairs)
{
#if OHA_NULL_POINTER_CHECKS
    if (tpht == NULL || next_pair == NULL) {
        return 0;
    }
#endif
    return oha_tpht_next_timeout_entries_int(tpht, next_pair, num_pairs);
}

OHA_PUBLIC_API void *
oha_tpht_update_time_for_entry(struct oha_tpht * const tpht, const void * const key, int64_t new_timestamp)
{
#if OHA_NULL_POINTER_CHECKS
    if (tpht == NULL || key == NULL) {
        return NULL;
    }
#endif
    return oha_tpht_update_time_for_entry_int(tpht, key, new_timestamp);
}

OHA_PUBLIC_API void *
oha_tpht_set_timeout_slot(struct oha_tpht * tpht, const void * key, uint8_t new_slot)
{
#if OHA_NULL_POINTER_CHECKS
    if (tpht == NULL) {
        return NULL;
    }
#endif
    return oha_tpht_set_timeout_slot_int(tpht, key, new_slot);
}

#endif
