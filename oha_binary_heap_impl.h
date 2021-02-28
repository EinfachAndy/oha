#ifndef OHA_BINARY_HEAP_H_
#define OHA_BINARY_HEAP_H_

#include "oha.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "oha_utils.h"

struct oha_bh_value_bucket {
    struct oha_bh_key_bucket * key;
    uint8_t value_buffer[];
};

struct oha_bh_key_bucket {
    int64_t key;
    struct oha_bh_value_bucket * value;
};

struct oha_bh {
    struct oha_bh_config config;
    size_t value_size;
    uint_fast32_t elems;
    struct oha_bh_value_bucket * values;
    struct oha_bh_key_bucket * keys;
};

static inline struct oha_bh_value_bucket * oha_bh_get_value_bucket(void * value)
{
    struct oha_bh_value_bucket * value_bucket =
        (struct oha_bh_value_bucket *)((uint8_t *)value - offsetof(struct oha_bh_value_bucket, value_buffer));
    assert(value_bucket->key->value == value_bucket);
    return value_bucket;
}

__attribute__((always_inline))
static inline uint_fast32_t oha_bh_parent(uint_fast32_t i)
{
    return (i - 1) / 2;
}

__attribute__((always_inline))
static inline uint_fast32_t oha_bh_left(uint_fast32_t i)
{
    return (2 * i + 1);
}

__attribute__((always_inline))
static inline uint_fast32_t oha_bh_right(uint_fast32_t i)
{
    return (2 * i + 2);
}

__attribute__((always_inline))
static inline void oha_bh_swap_keys(struct oha_bh_key_bucket * const restrict a,
                                    struct oha_bh_key_bucket * const restrict b)
{
    a->value->key = b;
    b->value->key = a;
    struct oha_bh_key_bucket tmp_a = *a;
    *a = *b;
    *b = tmp_a;
}

__attribute__((always_inline))
static inline void oha_bh_connect_keys_values(struct oha_bh * const heap)
{
    // connect keys and values
    struct oha_bh_value_bucket * tmp_value = heap->values;
    for (uint_fast32_t i = 0; i < heap->config.max_elems; i++) {
        heap->keys[i].value = tmp_value;
        tmp_value->key = &heap->keys[i];
        tmp_value = oha_move_ptr_num_bytes(tmp_value, heap->value_size);
    }
}

__attribute__((always_inline))
static inline bool oha_bh_resize_heap(struct oha_bh * const heap)
{
    if (!heap->config.resizable) {
        return false;
    }

    struct oha_memory_fp * memory = &heap->config.memory;
    struct oha_bh tmp_heap = {0};
    tmp_heap.config = heap->config;
    // TODO add 32 bit overflow check
    tmp_heap.config.max_elems *= 2;
    const struct oha_bh_config * config = &tmp_heap.config;

    tmp_heap.keys = oha_calloc(memory, config->max_elems * sizeof(struct oha_bh_key_bucket));
    if (tmp_heap.keys == NULL) {
        return false;
    }

    tmp_heap.value_size = OHA_ALIGN_UP(sizeof(struct oha_bh_value_bucket) + config->value_size);
    tmp_heap.values = oha_malloc(memory, config->max_elems * tmp_heap.value_size);
    if (tmp_heap.values == NULL) {
        oha_free(memory, tmp_heap.keys);
        return false;
    }

    oha_bh_connect_keys_values(&tmp_heap);

    int64_t tmp_key;
    while (OHA_BH_NOT_FOUND != (tmp_key = oha_bh_find_min(heap))) {
        void * value = oha_bh_delete_min(heap);
        void * new_value = oha_bh_insert(&tmp_heap, tmp_key);
        memcpy(new_value, value, config->value_size);
    }

    // destroy old table buffers
    oha_free(memory, heap->keys);
    oha_free(memory, heap->values);

    // copy new heap
    *heap = tmp_heap;

    return true;
}

static void oha_bh_heapify(struct oha_bh * const heap, uint_fast32_t i)
{
    uint_fast32_t l = oha_bh_left(i);
    uint_fast32_t r = oha_bh_right(i);
    uint_fast32_t smallest = i;
    if (l < heap->elems && heap->keys[l].key < heap->keys[i].key)
        smallest = l;
    if (r < heap->elems && heap->keys[r].key < heap->keys[smallest].key)
        smallest = r;
    if (smallest != i) {
        oha_bh_swap_keys(&heap->keys[i], &heap->keys[smallest]);
        oha_bh_heapify(heap, smallest);
    }
}

/*
 * public functions
 */

OHA_PUBLIC_API void oha_bh_destroy(struct oha_bh * const heap)
{
    if (heap == NULL) {
        return;
    }
    const struct oha_memory_fp * memory = &heap->config.memory;
    oha_free(memory, heap->keys);
    oha_free(memory, heap->values);
    oha_free(memory, heap);
}

OHA_PUBLIC_API struct oha_bh * oha_bh_create(const struct oha_bh_config * const config)
{
    if (config == NULL) {
        return NULL;
    }
    const struct oha_memory_fp * memory = &config->memory;

    struct oha_bh * const heap = oha_calloc(memory, sizeof(struct oha_bh));
    if (heap == NULL) {
        return NULL;
    }
    heap->config = *config;

    heap->keys = oha_calloc(memory, config->max_elems * sizeof(struct oha_bh_key_bucket));
    if (heap->keys == NULL) {
        oha_bh_destroy(heap);
        return NULL;
    }

    heap->value_size = OHA_ALIGN_UP(sizeof(struct oha_bh_value_bucket) + config->value_size);
    heap->values = oha_malloc(memory, config->max_elems * heap->value_size);
    if (heap->values == NULL) {
        oha_bh_destroy(heap);
        return NULL;
    }

    // connect keys and values
    oha_bh_connect_keys_values(heap);

    return heap;
}

OHA_PUBLIC_API void * oha_bh_insert(struct oha_bh * const heap, int64_t key)
{
    if (heap == NULL) {
        return NULL;
    }
    if (heap->elems >= heap->config.max_elems) {
        if (!oha_bh_resize_heap(heap)) {
            return NULL;
        }
    }

    // insert the new key at the end
    uint_fast32_t i = heap->elems;
    heap->keys[i].key = key;

    // Fix the min heap property if it is violated
    while (i != 0 && heap->keys[oha_bh_parent(i)].key > heap->keys[i].key) {
        oha_bh_swap_keys(&heap->keys[i], &heap->keys[oha_bh_parent(i)]);
        i = oha_bh_parent(i);
    }

    heap->elems++;
    return heap->keys[i].value->value_buffer;
}

OHA_PUBLIC_API int64_t oha_bh_find_min(const struct oha_bh * const heap)
{
    if (heap == NULL || heap->elems == 0) {
        return OHA_BH_NOT_FOUND;
    }
    if (heap->elems == 0) {
        return OHA_BH_NOT_FOUND;
    }

    return heap->keys[0].key;
}

OHA_PUBLIC_API void * oha_bh_delete_min(struct oha_bh * const heap)
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
    oha_bh_swap_keys(&heap->keys[0], &heap->keys[heap->elems]);
    oha_bh_heapify(heap, 0);

    // Swapped root entry
    return heap->keys[heap->elems].value->value_buffer;
}

OHA_PUBLIC_API void * oha_bh_remove(struct oha_bh * const heap, void * const value, int64_t * out_key)
{
    if (heap == NULL || value == NULL) {
        return NULL;
    }

    struct oha_bh_value_bucket * value_bucket = oha_bh_get_value_bucket(value);
    struct oha_bh_key_bucket * key = value_bucket->key;
    int64_t key_value = key->key;

    // move to top
    if (OHA_BH_MIN_VALUE != oha_bh_change_key(heap, value, OHA_BH_MIN_VALUE)) {
        return NULL;
    }
    // delete the swapped element
    if (oha_bh_delete_min(heap) != value_bucket->value_buffer) {
        return NULL;
    }

    if (out_key != NULL) {
        *out_key = key_value;
    }

    return value_bucket->value_buffer;
}

OHA_PUBLIC_API int64_t oha_bh_change_key(struct oha_bh * const heap, void * const value, int64_t new_val)
{
    if (heap == NULL || value == NULL) {
        return OHA_BH_NOT_FOUND;
    }

    struct oha_bh_value_bucket * value_bucket = oha_bh_get_value_bucket(value);

    enum mode {
        UNCHANGED_KEY,
        DECREASE_KEY,
        INCREASE_KEY,
    } mode;
    struct oha_bh_key_bucket * key = value_bucket->key;
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
            while (index > 0 && heap->keys[oha_bh_parent(index)].key > heap->keys[index].key) {
                oha_bh_swap_keys(&heap->keys[index], &heap->keys[oha_bh_parent(index)]);
                index = oha_bh_parent(index);
            }
            break;
        }
        case INCREASE_KEY: {
            // swap to bottom
            while (1) {
                // check for right side
                int64_t rigth_key = INT64_MIN;
                int64_t left_key = INT64_MIN;
                if (oha_bh_right(index) < heap->elems && heap->keys[oha_bh_right(index)].key < heap->keys[index].key) {
                    rigth_key = heap->keys[oha_bh_right(index)].key;
                }
                // check for left side
                if (oha_bh_left(index) < heap->elems && heap->keys[oha_bh_left(index)].key < heap->keys[index].key) {
                    left_key = heap->keys[oha_bh_left(index)].key;
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
                    oha_bh_swap_keys(&heap->keys[index], &heap->keys[oha_bh_left(index)]);
                    index = oha_bh_left(index);
                } else {
                    oha_bh_swap_keys(&heap->keys[index], &heap->keys[oha_bh_right(index)]);
                    index = oha_bh_left(index);
                }
            }
            break;
        }
    }

    return new_val;
}

#endif
