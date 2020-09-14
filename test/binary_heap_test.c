#include <stdlib.h>
#include <time.h>
#include <unity.h>

#include "oha.h"

/* Is run before every test, put unit init calls here. */
void setUp(void)
{
    unsigned int seed = time(NULL);
    srand(seed);
    fprintf(stderr, "pseudo-random number: %u\n", seed);
}
/* Is run after every test, put unit clean-up calls here. */
void tearDown(void)
{
}

int compare_int64_t(const void * a, const void * b)
{
    int64_t int_a = *((int64_t *)a);
    int64_t int_b = *((int64_t *)b);

    // an easy expression for comparing
    return (int_a > int_b) - (int_a < int_b);
}

static int64_t * get_random_array(size_t num_elems, unsigned int bound)
{
    int64_t * array = calloc(sizeof(uint64_t), num_elems);
    if (array == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < num_elems; i++) {
        array[i] = rand() % bound;
    }

    return array;
}

void test_create_destroy()
{
    const struct oha_bh_config config = {
        .value_size = 0,
        .max_elems = 100,
    };

    struct oha_bh * heap = oha_bh_create(&config);
    oha_bh_destroy(heap);
}

void test_insert_delete_min()
{
    const size_t array_size = 100 * 1000;
    const struct oha_bh_config config = {
        .value_size = 0,
        .max_elems = array_size,
    };

    struct oha_bh * heap = oha_bh_create(&config);
    int64_t * big_rand_array = get_random_array(array_size, 100 * 1000 * 1000);

    for (size_t i = 0; i < array_size; i++) {
        oha_bh_insert(heap, big_rand_array[i]);
    }

    qsort(big_rand_array, array_size, sizeof(int64_t), compare_int64_t);

    for (size_t i = 0; i < array_size; i++) {
        int64_t next_min = oha_bh_find_min(heap);
        TEST_ASSERT_EQUAL_INT64(big_rand_array[i], next_min);
        TEST_ASSERT_NOT_NULL(oha_bh_delete_min(heap));
    }

    TEST_ASSERT_EQUAL_INT64(0, oha_bh_find_min(heap));
    TEST_ASSERT_NULL(oha_bh_delete_min(heap));

    oha_bh_destroy(heap);
    free(big_rand_array);
}

void test_insert_delete_min_check_value_ptr()
{
    const size_t array_size = 10 * 1;
    const struct oha_bh_config config = {
        .value_size = sizeof(int64_t),
        .max_elems = array_size,
    };

    struct oha_bh * heap = oha_bh_create(&config);

    struct key_value_pair {
        int64_t key;
        void * value;
    };

    struct key_value_pair * big_rand_array = calloc(sizeof(struct key_value_pair), array_size);
    TEST_ASSERT_NOT_NULL_MESSAGE(big_rand_array, "could not create test array");

    for (size_t i = 0; i < array_size; i++) {
        big_rand_array[i].key = i; // rand() % 100000;
        big_rand_array[i].value = oha_bh_insert(heap, big_rand_array[i].key);
        fprintf(stderr, "%ld --> %p\n", big_rand_array[i].key, big_rand_array[i].value);
    }

    // qsort(big_rand_array, array_size, sizeof(struct key_value_pair), compare_int64_t);

    for (size_t i = 0; i < array_size; i++) {
        int64_t next_min = oha_bh_find_min(heap);
        TEST_ASSERT_EQUAL_INT64(big_rand_array[i].key, next_min);
        void * value_ptr = oha_bh_delete_min(heap);
        fprintf(stderr, "%ld --> %p\n", next_min, value_ptr);
        TEST_ASSERT_NOT_NULL(value_ptr);
        TEST_ASSERT_EQUAL_PTR(big_rand_array[i].value, value_ptr);
    }

    TEST_ASSERT_EQUAL_INT64(0, oha_bh_find_min(heap));
    TEST_ASSERT_NULL(oha_bh_delete_min(heap));

    oha_bh_destroy(heap);
    free(big_rand_array);
}

void test_decrease_key()
{
    const size_t array_size = 5;
    const struct oha_bh_config config = {
        .value_size = sizeof(int64_t),
        .max_elems = array_size,
    };
    struct oha_bh * heap = oha_bh_create(&config);

    uint64_t * array[array_size + 1];

    for (size_t i = 1; i <= array_size; i++) {
        array[i] = oha_bh_insert(heap, i);
        TEST_ASSERT_NOT_NULL(array[i]);
        *array[i] = i;
    }

    // decrease key from 3 to 0 and delete
    TEST_ASSERT_EQUAL(1, oha_bh_find_min(heap));
    TEST_ASSERT_EQUAL(0, oha_bh_change_key(heap, array[3], 0));
    TEST_ASSERT_EQUAL(0, oha_bh_find_min(heap));
    uint64_t * ptr = oha_bh_delete_min(heap);
    TEST_ASSERT_EQUAL_PTR(array[3], ptr);
    TEST_ASSERT_EQUAL(3, *array[3]);
    TEST_ASSERT_EQUAL(1, oha_bh_find_min(heap));

    // delete key 1
    ptr = oha_bh_delete_min(heap);
    TEST_ASSERT_EQUAL_PTR(array[1], ptr);
    TEST_ASSERT_EQUAL(1, *array[1]);
    TEST_ASSERT_EQUAL(2, oha_bh_find_min(heap));

    // decrease key from 5 to 1 and delete
    TEST_ASSERT_EQUAL(1, oha_bh_change_key(heap, array[5], 1));
    TEST_ASSERT_EQUAL(1, oha_bh_find_min(heap));
    ptr = oha_bh_delete_min(heap);
    TEST_ASSERT_EQUAL_PTR(array[5], ptr);
    TEST_ASSERT_EQUAL(5, *array[5]);
    TEST_ASSERT_EQUAL(2, oha_bh_find_min(heap));

    // delete key 2
    ptr = oha_bh_delete_min(heap);
    TEST_ASSERT_EQUAL_PTR(array[2], ptr);
    TEST_ASSERT_EQUAL(2, *array[2]);
    TEST_ASSERT_EQUAL(4, oha_bh_find_min(heap));

    // delete key 4
    ptr = oha_bh_delete_min(heap);
    TEST_ASSERT_EQUAL_PTR(array[4], ptr);
    TEST_ASSERT_EQUAL(4, *array[4]);
    TEST_ASSERT_EQUAL(0, oha_bh_find_min(heap));
    TEST_ASSERT_NULL(oha_bh_delete_min(heap));

    oha_bh_destroy(heap);
}

void test_increase_key()
{
    const size_t array_size = 80;
    const struct oha_bh_config config = {
        .value_size = sizeof(int64_t),
        .max_elems = array_size,
    };
    struct oha_bh * heap = oha_bh_create(&config);

    uint64_t * array[array_size + 1];

    for (size_t i = 1; i <= array_size; i++) {
        array[i] = oha_bh_insert(heap, i);
        TEST_ASSERT_NOT_NULL(array[i]);
        *array[i] = i;
    }

    // increase key from 1 to 10 and delete
    TEST_ASSERT_EQUAL(1, oha_bh_find_min(heap));
    TEST_ASSERT_EQUAL(100, oha_bh_change_key(heap, array[1], 100));
    TEST_ASSERT_EQUAL(2, oha_bh_find_min(heap));

    uint64_t * ptr = NULL;
    for (size_t i = 2; i <= array_size; i++) {
        ptr = oha_bh_delete_min(heap);
        TEST_ASSERT_EQUAL_PTR(array[i], ptr);
        TEST_ASSERT_EQUAL(i, *array[i]);
        if (i < array_size) {
            TEST_ASSERT_EQUAL(i + 1, oha_bh_find_min(heap));
        } else {
            TEST_ASSERT_EQUAL(100, oha_bh_find_min(heap));
        }
    }

    // delete key 1
    ptr = oha_bh_delete_min(heap);
    TEST_ASSERT_EQUAL_PTR(array[1], ptr);
    TEST_ASSERT_EQUAL(1, *array[1]);
    TEST_ASSERT_EQUAL(0, oha_bh_find_min(heap));

    oha_bh_destroy(heap);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_create_destroy);
    RUN_TEST(test_insert_delete_min);
    RUN_TEST(test_insert_delete_min_check_value_ptr);
    RUN_TEST(test_decrease_key);
    RUN_TEST(test_increase_key);

    return UNITY_END();
}
