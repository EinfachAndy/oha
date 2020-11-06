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

/**********************************************************************************************************************
 *  linear probing hash table (lpht)
 *
 *      - TODO add docu
 *
 **********************************************************************************************************************/
struct oha_lpht;

struct oha_lpht_config {
    double load_factor;
    size_t key_size;
    size_t value_size;
    uint32_t max_elems;
};

size_t oha_lpht_calculate_size(const struct oha_lpht_config * config);
struct oha_lpht * oha_lpht_initialize(const struct oha_lpht_config * config, void * memory);
struct oha_lpht * oha_lpht_create(const struct oha_lpht_config * config);
void oha_lpht_destroy(struct oha_lpht * table);
void * oha_lpht_look_up(struct oha_lpht * table, const void * key);
void * oha_lpht_insert(struct oha_lpht * table, const void * key);
void * oha_lpht_get_key_from_value(const void * value);
void * oha_lpht_remove(struct oha_lpht * table, const void * key);
void oha_lpht_clear(struct oha_lpht * table);
struct oha_key_value_pair oha_lpht_get_next_element_to_remove(struct oha_lpht * table);

/**********************************************************************************************************************
 *  binary heap (bh)
 *
 *      - TODO add docu
 *
 **********************************************************************************************************************/
struct oha_bh_config {
    size_t value_size;
    uint32_t max_elems;
};
struct oha_bh;
size_t oha_bh_calculate_size(const struct oha_bh_config * config);
struct oha_bh * oha_bh_initialize(const struct oha_bh_config * config, void * memory);
struct oha_bh * oha_bh_create(const struct oha_bh_config * config);
void oha_bh_destroy(struct oha_bh * heap);
int64_t oha_bh_find_min(struct oha_bh * heap);
void * oha_bh_delete_min(struct oha_bh * heap);
void * oha_bh_insert(struct oha_bh * heap, int64_t key);
int64_t oha_bh_change_key(struct oha_bh * heap, void * value, int64_t new_val);

/**********************************************************************************************************************
 *  pht (prioritized hash table)
 *
 *      - TODO add docu
 *
 **********************************************************************************************************************/
struct oha_pht_config;
struct oha_pht;
size_t oha_pht_calculate_size(const struct oha_pht_config * config);
struct oha_pht * oha_pht_initialize(const struct oha_pht_config * config, void * memory);
struct oha_pht * oha_pht_create(void);
void oha_pht_destroy(struct oha_pht * pht);
void * oha_pht_insert(struct oha_pht * pht, void * key, int64_t heap_key);
void * oha_pht_look_up(struct oha_pht * pht, void * key);
void * oha_pht_remove(struct oha_pht * pht, void * key);
struct oha_key_value_pair oha_pht_find_min(struct oha_pht * pht);
struct oha_key_value_pair oha_pht_delete_min(struct oha_pht * pht);
struct oha_key_value_pair oha_pht_decrease_key(struct oha_pht *, void * key, int64_t new_heap_key);

#ifdef __cplusplus
}
#endif

#endif
