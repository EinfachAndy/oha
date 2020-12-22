#include "oha.h"

#include <stdlib.h>
#include <limits.h>

#include "utils.h"

struct storage_info {
    size_t hash_table_size;
    uint8_t number_of_timeout_slots;
};

struct timeout_slot {
    struct oha_bh * bh;
    int64_t timeout;
};

struct oha_tpht {
    struct oha_lpht * lpht;
    struct timeout_slot * slots;
    size_t num_timeout_slots;
    int64_t last_timestamp;
    struct storage_info storage;
    struct oha_lpht_config lpht_config;
};

struct oha_tpht * oha_tpht_create(const struct oha_tpht_config * const config)
{
    if (config == NULL) {
        return NULL;
    }

    struct oha_tpht * table = oha_calloc(&config->lpht_config.memory, sizeof(struct oha_tpht));
    if (table == NULL) {
        return NULL;
    }
    table->lpht = oha_lpht_create(&config->lpht_config);

    return table;
}

int8_t oha_tpht_add_timeout_slot(struct oha_tpht * const tpht, int64_t timeout, uint32_t num_elements, bool resizable)
{
    if (tpht == NULL) {
        return -1;
    }

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

    struct timeout_slot * next_free_slot = &tpht->slots[next_free_index];

    next_free_slot->bh = oha_bh_create(&bh_config);
    next_free_slot->timeout = timeout;
    if (next_free_slot->bh == NULL) {
        oha_remove_entry_from_array(&tpht->lpht_config.memory,
                                    (void *)&tpht->slots,
                                    sizeof(*tpht->slots),
                                    &tpht->num_timeout_slots,
                                    next_free_index);
    }

    return tpht->num_timeout_slots;
}

void oha_tpht_destroy(struct oha_tpht * const tpht)
{
    if (tpht == NULL) {
        return;
    }

    oha_lpht_destroy(tpht->lpht);
    tpht->lpht = NULL;

    for (size_t i = 0; i < tpht->num_timeout_slots; i++) {
        oha_bh_destroy(tpht->slots[i].bh);
    }
    oha_free(&tpht->lpht_config.memory, tpht->slots);
    tpht->slots = NULL;

    oha_free(&tpht->lpht_config.memory, tpht);
}

void * oha_tpht_insert(struct oha_tpht * const pht, const void * const key, int64_t timestamp, uint8_t timeout_slot_id)
{
    (void)pht;
    (void)key;
    (void)timestamp;
    (void)timeout_slot_id;
    return NULL;
}

void * oha_tpht_look_up(const struct oha_tpht * const pht, const void * const key)
{
    (void)pht;
    (void)key;

    return NULL;
}

void * oha_tpht_remove(struct oha_tpht * const pht, const void * const key)
{
    (void)pht;
    (void)key;
    return NULL;
}

bool oha_tpht_next_timeout_entry(struct oha_tpht * const pht, struct oha_key_value_pair * const next_pair)
{
    (void)pht;
    (void)next_pair;

    return false;
}

void * oha_tpht_update_time_for_entry(struct oha_tpht * const pht, const void * const key, int64_t new_timestamp)
{
    (void)pht;
    (void)key;
    (void)new_timestamp;
    return NULL;
}
