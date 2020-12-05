#include "oha.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

struct value_bucket {
    struct key_bucket * key;
    uint8_t value_buffer[];
};

struct key_bucket {
    int64_t key;
    struct value_bucket * value;
};

struct oha_bh {
    struct oha_bh_config config;
    size_t value_size;
    uint_fast32_t elems;
    struct value_bucket * values;
    struct key_bucket * keys;
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

static inline void swap_keys(struct key_bucket * restrict a, struct key_bucket * restrict b)
{
    a->value->key = b;
    b->value->key = a;
    struct key_bucket tmp_a = *a;
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
    if (heap == NULL) {
        return;
    }
    const struct oha_memory_fp * memory = &heap->config.memory;
    oha_free(memory, heap->keys);
    oha_free(memory, heap->values);
    oha_free(memory, heap);
}

struct oha_bh * oha_bh_create(const struct oha_bh_config * config)
{
    if (config == NULL) {
        return NULL;
    }
    const struct oha_memory_fp * memory = &config->memory;

    struct oha_bh * heap = oha_calloc(memory, sizeof(struct oha_bh));
    if (heap == NULL) {
        return NULL;
    }
    heap->config = *config;

    heap->keys = oha_calloc(memory, config->max_elems * sizeof(struct key_bucket));
    if (heap->keys == NULL) {
        oha_bh_destroy(heap);
        return NULL;
    }

    heap->value_size = sizeof(struct value_bucket) + config->value_size;
    heap->values = oha_calloc(memory, config->max_elems * heap->value_size);
    if (heap->values == NULL) {
        oha_bh_destroy(heap);
        return NULL;
    }

    // connect keys and values
    struct value_bucket * tmp_value = heap->values;
    for (uint_fast32_t i = 0; i < config->max_elems; i++) {
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
    if (heap->elems >= heap->config.max_elems) {
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
    return heap->keys[i].value->value_buffer;
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
        return heap->keys[0].value->value_buffer;
    }

    heap->elems--;
    swap_keys(&heap->keys[0], &heap->keys[heap->elems]);
    heapify(heap, 0);

    // Swapped root entry
    return heap->keys[heap->elems].value->value_buffer;
}

int64_t oha_bh_change_key(struct oha_bh * heap, void * value, int64_t new_val)
{
    if (heap == NULL || value == NULL) {
        return 0;
    }

    struct value_bucket * value_bucket =
        (struct value_bucket *)((uint8_t *)value - offsetof(struct value_bucket, value_buffer));
    assert(value_bucket->key->value == value_bucket);

    enum mode {
        UNCHANGED_KEY,
        DECREASE_KEY,
        INCREASE_KEY,
    } mode;
    struct key_bucket * key = value_bucket->key;
    if (new_val < key->key) {
        mode = DECREASE_KEY;
    } else if (new_val == key->key) {
        mode = UNCHANGED_KEY;
    } else {
        mode = INCREASE_KEY;
    }

    key->key = new_val;
    uint_fast32_t index = key - heap->keys;

    switch (mode) {
        case UNCHANGED_KEY:
            // nothing todo
            break;
        case DECREASE_KEY: {
            // swap to top
            while (index > 0 && heap->keys[parent(index)].key > heap->keys[index].key) {
                swap_keys(&heap->keys[index], &heap->keys[parent(index)]);
                index = parent(index);
            }
            break;
        }
        case INCREASE_KEY: {
            // swap to bottom
            while (1) {
                // check for right side
                int64_t rigth_key = INT64_MIN;
                int64_t left_key = INT64_MIN;
                if (right(index) < heap->elems && heap->keys[right(index)].key < heap->keys[index].key) {
                    rigth_key = heap->keys[right(index)].key;
                }
                // check for left side
                if (left(index) < heap->elems && heap->keys[left(index)].key < heap->keys[index].key) {
                    left_key = heap->keys[left(index)].key;
                }

                // no element to swap
                if (rigth_key == INT64_MIN && left_key == INT64_MIN) {
                    break;
                }

                // check for smallest element which is not INT64_MIN
                bool take_left;
                if (rigth_key == INT64_MIN) {
                    take_left = true;
                } else if (left_key == INT64_MIN) {
                    take_left = false;
                } else if (left_key < rigth_key) {
                    take_left = true;
                } else {
                    take_left = false;
                }

                // swap
                if (take_left) {
                    swap_keys(&heap->keys[index], &heap->keys[left(index)]);
                    index = left(index);
                } else {
                    swap_keys(&heap->keys[index], &heap->keys[right(index)]);
                    index = left(index);
                }
            }
            break;
        }
    }

    return new_val;
}
