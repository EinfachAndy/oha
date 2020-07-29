#include "oha.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#define CMP(a, b) ((a) >= (b))

struct oha_bh_value_bucket {
    struct oha_bh_key_bucket * key;
    uint8_t value_data[];
};

struct oha_bh_key_bucket {
    int64_t key;
    struct oha_bh_value_bucket * value;
};

struct oha_bh {
    size_t value_size;
    uint_fast32_t max_elems;
    uint_fast32_t elems;
    struct oha_bh_value_bucket * values;
    struct oha_bh_key_bucket * keys;
};

static inline uint_fast32_t parent(uint_fast32_t i)
{
    return (i - 1) / 2;
}

static inline uint_fast32_t left(uint_fast32_t i)
{
    return (2 * i + 1);
}

static inline uint_fast32_t right(uint_fast32_t i)
{
    return (2 * i + 2);
}

static inline void swap_keys(struct oha_bh_key_bucket * restrict a, struct oha_bh_key_bucket * restrict b)
{
    a->value->key = b;
    b->value->key = a;
    struct oha_bh_key_bucket tmp_a = *a;
    *a = *b;
    *b = tmp_a;
}

static void heapify(struct oha_bh * heap, uint_fast32_t i)
{
    uint_fast32_t l = left(i);
    uint_fast32_t r = right(i);
    uint_fast32_t smallest = i;
    if (l < heap->elems && heap->keys[l].key < heap->keys[i].key)
        smallest = l;
    if (r < heap->elems && heap->keys[r].key < heap->keys[smallest].key)
        smallest = r;
    if (smallest != i) {
        swap_keys(&heap->keys[i], &heap->keys[smallest]);
        heapify(heap, smallest);
    }
}

void oha_bh_destroy(struct oha_bh * heap)
{
    free(heap->keys);
    free(heap->values);
    free(heap);
}

struct oha_bh * oha_bh_create(const struct oha_bh_config * config)
{
    if (config == NULL) {
        return NULL;
    }
    struct oha_bh * heap = calloc(1, sizeof(struct oha_bh));
    if (heap == NULL) {
        return NULL;
    }

    heap->keys = calloc(config->max_elems, sizeof(struct oha_bh_key_bucket));
    if (heap->keys == NULL) {
        oha_bh_destroy(heap);
        return NULL;
    }

    heap->value_size = sizeof(struct oha_bh_value_bucket) + config->value_size;
    heap->values = calloc(config->max_elems, heap->value_size);
    if (heap->values == NULL) {
        oha_bh_destroy(heap);
        return NULL;
    }
    heap->max_elems = config->max_elems;

    // connect keys and values
    struct oha_bh_value_bucket * tmp_value = heap->values;
    for (uint_fast32_t i = 0; i < heap->max_elems; i++) {
        heap->keys[i].value = tmp_value;
        tmp_value->key = &heap->keys[i];
        tmp_value = move_ptr_num_bytes(tmp_value, heap->value_size);
    }

    return heap;
}

void * oha_bh_insert(struct oha_bh * heap, int64_t key)
{
    if (heap == NULL) {
        return NULL;
    }
    if (heap->elems >= heap->max_elems) {
        return NULL;
    }

    // insert the new key at the end
    uint_fast32_t i = heap->elems;
    heap->keys[i].key = key;

    // Fix the min heap property if it is violated
    while (i != 0 && heap->keys[parent(i)].key > heap->keys[i].key) {
        swap_keys(&heap->keys[i], &heap->keys[parent(i)]);
        i = parent(i);
    }

    heap->elems++;
    return heap->keys[i].value->value_data;
}

int64_t oha_bh_find_min(struct oha_bh * heap)
{
    if (heap == NULL || heap->elems == 0) {
        return 0;
    }
    return heap->keys[0].key;
}

void * oha_bh_delete_min(struct oha_bh * heap)
{
    if (heap == NULL) {
        return NULL;
    }
    if (heap->elems == 0) {
        return NULL;
    }
    if (heap->elems == 1) {
        heap->elems--;
        return heap->keys[0].value;
    }

    heap->elems--;
    swap_keys(&heap->keys[0], &heap->keys[heap->elems]);
    heapify(heap, 0);

    // Swapped root entry
    return heap->keys[heap->elems].value;
}

int64_t oha_bh_decrease_key(struct oha_bh * heap, void * value, int64_t new_val)
{
    if (heap == NULL || value == NULL) {
        return 0;
    }

    struct oha_bh_value_bucket * value_bucket =
        (struct oha_bh_value_bucket *)((uint8_t *)value) - offsetof(struct oha_bh_value_bucket, value_data);
    assert(value_bucket->value_data == value);

    struct oha_bh_key_bucket * key = value_bucket->key;
    if (key->key <= new_val) {
        // nothing todo
        return key->key;
    }

    key->key = new_val;
    uint_fast32_t i = (key - heap->keys) / sizeof(struct oha_bh_key_bucket);

    while (i != 0 && heap->keys[parent(i)].key > heap->keys[i].key) {
        swap_keys(&heap->keys[i], &heap->keys[parent(i)]);
        i = parent(i);
    }
    return new_val;
}