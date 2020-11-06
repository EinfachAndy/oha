#include "oha.h"

#include <stdlib.h>

struct oha_tpht {
    struct oha_lpht * lpht;
    struct oha_bh * bh;
    int64_t last_timestamp;
};

struct storage_info {
    size_t hash_table_size;
};

struct oha_tpht * oha_tpht_create(const struct oha_tpht_config * config)
{
    if (config == NULL) {
        return NULL;
    }

    if (config->number_of_timeout_slots == 0 || config->number_of_timeout_slots > OHA_MAX_TIMEOUT_SLOTS) {
        return NULL;
    }

    struct oha_tpht * table = calloc(1, sizeof(struct oha_tpht));
    if (table == NULL) {
        return NULL;
    }
    table->lpht = oha_lpht_create(&config->lpht_config);

    struct oha_bh_config bh_config = {
        .value_size = config->lpht_config.key_size,
        .max_elems = config->lpht_config.max_elems,
    };

    table->bh = oha_bh_create(&bh_config);
    if (table->bh == NULL) {
        oha_lpht_destroy(table->lpht);
        return NULL;
    }

    return table;
}

void oha_tpht_destroy(struct oha_tpht * pht)
{
    if (pht == NULL) {
        return;
    }
    oha_lpht_destroy(pht->lpht);
    oha_bh_destroy(pht->bh);
}

void * oha_tpht_insert(struct oha_tpht * pht, void * key, int64_t timestamp)
{
    (void)pht;
    (void)key;
    (void)timestamp;
    return NULL;
}

void * oha_tpht_look_up(struct oha_tpht * pht, void * key)
{
    (void)pht;
    (void)key;

    return NULL;
}

void * oha_tpht_remove(struct oha_tpht * pht, void * key)
{
    (void)pht;
    (void)key;
    return NULL;
}

bool oha_tpht_next_timeout_entry(struct oha_tpht * pht, struct oha_key_value_pair * next_pair)
{
    (void)pht;
    (void)next_pair;

    return false;
}

void * oha_tpht_update_time_for_entry(struct oha_tpht * pht, void * key, int64_t new_timestamp)
{
    (void)pht;
    (void)key;
    (void)new_timestamp;
    return NULL;
}
