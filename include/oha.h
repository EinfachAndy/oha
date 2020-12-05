#ifndef ORDERED_HASHING_H_
#define ORDERED_HASHING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    double load_factor;
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
};

struct oha_lpht * oha_lpht_create(const struct oha_lpht_config * config);
void oha_lpht_destroy(struct oha_lpht * table);
void * oha_lpht_look_up(struct oha_lpht * table, const void * key);
void * oha_lpht_insert(struct oha_lpht * table, const void * key);
void * oha_lpht_get_key_from_value(const void * value);
void * oha_lpht_remove(struct oha_lpht * table, const void * key);
bool oha_lpht_get_status(struct oha_lpht * table, struct oha_lpht_status * status);
void oha_lpht_clear(struct oha_lpht * table);
bool oha_lpht_get_next_element_to_remove(struct oha_lpht * table, struct oha_key_value_pair * pair);

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
struct oha_bh;
struct oha_bh * oha_bh_create(const struct oha_bh_config * config);
void oha_bh_destroy(struct oha_bh * heap);
int64_t oha_bh_find_min(struct oha_bh * heap);
void * oha_bh_delete_min(struct oha_bh * heap);
void * oha_bh_insert(struct oha_bh * heap, int64_t key);
int64_t oha_bh_change_key(struct oha_bh * heap, void * value, int64_t new_val);

/**********************************************************************************************************************
 *  tpht (temporal prioritized hash table)
 *
 *      - TODO add docu
 *
 **********************************************************************************************************************/
struct oha_tpht;
#define OHA_MAX_TIMEOUT_SLOTS 10
struct oha_tpht_config {
    struct oha_lpht_config lpht_config;
    int64_t timeout_slots[OHA_MAX_TIMEOUT_SLOTS];
    size_t number_of_timeout_slots;
};

struct oha_tpht * oha_tpht_create(const struct oha_tpht_config * config);
void oha_tpht_destroy(struct oha_tpht * pht);
void * oha_tpht_insert(struct oha_tpht * pht, void * key, int64_t timestamp);
void * oha_tpht_look_up(struct oha_tpht * pht, void * key);
void * oha_tpht_remove(struct oha_tpht * pht, void * key);
void * oha_tpht_set_timeout_slot(struct oha_tpht * pht, void * key, size_t timeout_slot_id);
bool oha_tpht_next_timeout_entry(struct oha_tpht * pht, struct oha_key_value_pair * next_pair);
void * oha_tpht_update_time_for_entry(struct oha_tpht * pht, void * key, int64_t new_timestamp);

#ifdef __cplusplus
}
#endif

#endif
